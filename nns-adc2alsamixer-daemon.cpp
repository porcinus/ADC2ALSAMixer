/*
NNS @ 2018
nns-adc2alsamixer-daemon
Control ALSA mixer using ADC input (MCP3021A/MCP3221A)
*/
const char programversion[]="0.1a";

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <cstring>
#include <limits.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <alsa/asoundlib.h>



int debug=0; //program is in debug mode, 0=no 1=full
#define debug_print(fmt, ...) do { if (debug) fprintf(stderr, "%s:%d:%s(): " fmt, __FILE__, __LINE__, __func__, ##__VA_ARGS__); } while (0) //Flavor: print advanced debug to stderr

int i2c_bus=-1;									//i2c bus id
char i2c_path[PATH_MAX];				//path to i2c bus
int i2c_addr=0x4D;							//i2c device adress
int update_interval=250;				//update interval in msec
int update_forced_loops=40;			//loops interval for forced update
int update_forced_counter=0;		//loops count


bool i2c_addr_valid=false;			//i2c device adress is valid
int i2c_handle;									//i2c handle io
char i2c_buffer[10]={0};				//i2c data buffer
int i2c_retry=0;								//reading retry if failure

int adc_raw_value=0;						//adc value
int adc_last_value=-1;					//adc last value
bool adc_reverse=false;					//reverse adc value
int adc_low_value=0;						//adc min value
int adc_high_value=4095;				//adc max value
int adc_update_value=50;				//adc update value
bool test_mode=false;						//test mode
int adc_debug_low_value=-1;			//adc debug min value
int adc_debug_high_value=-1;		//adc debug max value

snd_mixer_t *handle = NULL;				//alsa handle
snd_mixer_elem_t *elem;						//alsa element
snd_mixer_selem_id_t *sid;				//alsa selector
int alsa_err=0;										//alsa error
long tmp_min_db,tmp_max_db;				//real alsa min max
long alsa_low_value=-1;						//alsa min value
long alsa_high_value=-1;					//alsa max value
bool alsa_low_defined = false; 		//alsa min value defined
bool alsa_high_defined = false; 	//alsa max value defined
long alsa_value=0;								//alsa value
char alsa_card[256];							//alsa card
char alsa_name[256];							//alsa name

bool MCP3021A_detected=false;			//chip detected bool


int nns_map_int(int x,int in_min,int in_max,int out_min,int out_max){
	if(x<in_min){return out_min;}
	if(x>in_max){return out_max;}
	return (x-in_min)*(out_max-out_min)/(in_max-in_min)+out_min;
}


void show_usage(void){
	printf(
"Example : ./nns-adc2alsamixer-daemon -test -debug 1 -i2caddr 0x4D -adcmin 150 -adcmax 4095 -alsacard \"default\" -alsaname \"Master\" -alsamin 0 -alsamax 80\n"
"Version: %s\n"
"Options:\n"
"\t-i2cbus, I2C bus id, scan thru all available if not set [Default: 1]\n"
"\t-i2caddr, I2C ADC device adress, found via 'i2cdetect' [Default: 0x4D]\n"
"\t-adcmin, ADC minimal value, min:0 [Default: 0]\n"
"\t-adcmax, ADC maximal value, max:4095 [Default: 4095]\n"
"\t-adcreverse, reverse ADC value if set\n"
"\t-alsamin, ALSA mixer min dB, should be negative [Default based on alsamixer]\n"
"\t-alsamax, ALSA mixer max dB, should be equal to 0 or higher [Default based on alsamixer]\n"
"\t-alsacard, ALSA card [Default: default]\n"
"\t-alsaname, ALSA selector name [Default: Master]\n"
"\t-updaterate, Time between each update, in msec [Default: 250]\n"
"\t-forcedupdate, Loops between each forced update [Default: 40]\n"
"\t-test, Enable test mode if set and report min/max ADC value\n"
"\t-debug, optional, 1=full(will spam logs), 0 if not set\n"
,programversion);
}


int main(int argc, char *argv[]){ //main
	setbuf(stdout,NULL); //unbuffered stdout to allow rewrite on the same line
	strcpy(alsa_card,"default"); //init
	strcpy(alsa_name,"Master"); //init
	
	for(int i=1;i<argc;++i){ //argument to variable
		if(strcmp(argv[i],"-help")==0){show_usage();return 1;
		}else if(strcmp(argv[i],"-debug")==0){debug=atoi(argv[i+1]);
		}else if(strcmp(argv[i],"-i2cbus")==0){i2c_bus=atoi(argv[i+1]);if(strstr(argv[i+1],"-")){i2c_bus=-i2c_bus;}
		}else if(strcmp(argv[i],"-i2caddr")==0){sscanf(argv[i+1], "%x", &i2c_addr);
		}else if(strcmp(argv[i],"-test")==0){test_mode=true;
		}else if(strcmp(argv[i],"-adcreverse")==0){adc_reverse=true;
		}else if(strcmp(argv[i],"-adcmin")==0){adc_low_value=atoi(argv[i+1]);
		}else if(strcmp(argv[i],"-adcmax")==0){adc_high_value=atoi(argv[i+1]);
		}else if(strcmp(argv[i],"-alsamin")==0){alsa_low_defined=true;alsa_low_value=(long)(atof(argv[i+1])*100);
		}else if(strcmp(argv[i],"-alsamax")==0){alsa_high_defined=true;alsa_high_value=(long)(atof(argv[i+1])*100);
		}else if(strcmp(argv[i],"-alsacard")==0){strcpy(alsa_card,argv[i+1]);
		}else if(strcmp(argv[i],"-alsaname")==0){strcpy(alsa_name,argv[i+1]);
		}else if(strcmp(argv[i],"-forcedupdate")==0){update_forced_loops=atoi(argv[i+1]);
		}else if(strcmp(argv[i],"-updaterate")==0){update_interval=atoi(argv[i+1]);}
	}
	
	if(adc_low_value<0){debug_print("Invalid ADC low value, setting it to 0\n"); adc_low_value=0;}
	if(adc_high_value>4095){debug_print("Invalid ADC high value, setting it to 4095\n"); adc_high_value=4095;}
	
	if(test_mode){printf("Running on test mode, please move the volume wheel multiples time until values no more change.\nOnce done, use these values on -adcmin and -adcmax arguments.\n\n");}
	
	for(int i=(i2c_bus<0)?0:i2c_bus;i<10;i++){ //detect i2c bus
		sprintf(i2c_path,"/dev/i2c-%d",i); //generate i2c bus full path
		if((i2c_handle=open(i2c_path,O_RDWR))>=0){ //open i2c handle
			if(ioctl(i2c_handle,I2C_SLAVE,i2c_addr)>=0){ //i2c adress detected
				if(read(i2c_handle,i2c_buffer,2)==2){ //success read
					i2c_bus=i; i2c_addr_valid=true;
					break; //exit loop
				}
			}
			close(i2c_handle); //close i2c handle
		}
	}
	
	if(i2c_addr_valid){debug_print("Bus %d : 0x%02x detected\n",i2c_bus,i2c_addr); //bus detected
	}else{debug_print("Failed, 0x%02x not detected on any bus, Exiting\n",i2c_addr);return(1);} //failed
	
	if(i2c_addr>=0x48&&i2c_addr<=0x4F){MCP3021A_detected=true; debug_print("MCP3021A detected\n");
	}else{debug_print("Failed, I2C address out of range, Exiting\n"); return(1);} //failed
  
	adc_update_value=(adc_high_value-adc_low_value)/100; //set right adc update value
	debug_print("ADC update value : %d\n",adc_update_value);
  
  if(!test_mode){ //initialize alsa mixer if not in test mode
		snd_mixer_selem_id_alloca(&sid); //allocate an invalid snd_mixer_selem_id_t using standard alloca
		snd_mixer_selem_id_set_index(sid,0); //set index part of a mixer simple element identifier
		snd_mixer_selem_id_set_name(sid,alsa_name); //set name part of a mixer simple element identifier
		
		if((alsa_err=snd_mixer_open(&handle,0))<0){debug_print("ALSA Mixer '%s' open error : %s\n",alsa_card,snd_strerror(alsa_err)); return alsa_err; //failed
		}else{debug_print("ALSA Mixer '%s' openned\n",alsa_card);}
		
		if((alsa_err=snd_mixer_attach(handle,alsa_card))<0){debug_print("ALSA Mixer attach '%s' error : %s",alsa_card,snd_strerror(alsa_err)); snd_mixer_close(handle); handle=NULL; return alsa_err; //failed
		}else{debug_print("ALSA Mixer '%s' attached\n",alsa_card);}
		
		if((alsa_err=snd_mixer_selem_register(handle,NULL,NULL))<0){debug_print("ALSA Mixer register error : %s",snd_strerror(alsa_err)); snd_mixer_close(handle); handle=NULL; return alsa_err; //failed
		}else{debug_print("ALSA Mixer registered\n",alsa_card);}
		
		alsa_err=snd_mixer_load(handle); //Load a mixer elements
		if(alsa_err<0){debug_print("ALSA Mixer '%s' load error : %s",alsa_card, snd_strerror(alsa_err)); snd_mixer_close(handle); handle=NULL; return alsa_err; //failed
		}else{debug_print("ALSA Mixer '%s' loaded\n",alsa_card);}
		
		elem=snd_mixer_find_selem(handle,sid); //find a mixer simple element
		if(!elem){debug_print("ALSA Mixer control not found : '%s,%i'\n",snd_mixer_selem_id_get_name(sid),snd_mixer_selem_id_get_index(sid)); snd_mixer_close(handle); handle=NULL; return -ENOENT; //failed
		}else{debug_print("ALSA Mixer control found : '%s,%i'\n",snd_mixer_selem_id_get_name(sid),snd_mixer_selem_id_get_index(sid));}
		
		snd_mixer_selem_get_playback_volume_range(elem,&tmp_min_db,&tmp_max_db); //get range for playback volume of a mixer simple element
		if(!alsa_low_defined){alsa_low_value=tmp_min_db; alsa_low_defined=true;}
		if(!alsa_high_defined){alsa_high_value=tmp_max_db; alsa_high_defined=true;}
		debug_print("ALSA Mixer dB range : min:%.2f dB, max:%.2f dB\n",(float)tmp_min_db/100,(float)tmp_max_db/100);
		debug_print("ALSA Mixer user range : min:%.2f dB, max:%.2f dB\n",(float)alsa_low_value/100,(float)alsa_high_value/100);
	}
	
	debug_print("Starting main loop\n");
	
	while(true){
		i2c_retry=0; //reset retry counter
		while(i2c_retry<3){
			adc_raw_value=-1; //reset adc raw value
			if((i2c_handle=open(i2c_path,O_RDWR))<0){
				debug_print("Failed to open the I2C bus : %s, retry in %dmsec\n",i2c_path,update_interval);
				i2c_retry=3; //no need to retry since failed to open I2C bus itself
			}else{
				if(ioctl(i2c_handle,I2C_SLAVE,i2c_addr)<0){ //access i2c device, allow retry if failed
					debug_print("Failed to access I2C device : %02x, retry in 1sec\n",i2c_addr);
				}else{ //success
					if(MCP3021A_detected){
						if(read(i2c_handle,i2c_buffer,2)!=2){debug_print("Failed to read data from I2C device : %04x, retry in 1sec\n",i2c_addr);
						}else{adc_raw_value=(i2c_buffer[0]<<8)|(i2c_buffer[1]&0xff);} //success, combine buffer bytes into integer
					}
					
					if(adc_raw_value<0){debug_print("Warning, ADC return wrong data : %d\n",adc_raw_value);
					}else{ //success
						if(test_mode){
							if(adc_debug_low_value==-1){adc_debug_low_value=adc_raw_value;} //update low value
							if(adc_debug_high_value==-1){adc_debug_high_value=adc_raw_value;} //update high value
							if(adc_raw_value<adc_debug_low_value){adc_debug_low_value=adc_raw_value;} //update min debug value
							if(adc_raw_value>adc_debug_high_value){adc_debug_high_value=adc_raw_value;} //update max debug value
							printf("\033[2K\rADC : min:%d , max:%d",adc_debug_low_value,adc_debug_high_value);
						}else{
							if((abs(adc_raw_value-adc_last_value)>adc_update_value||adc_last_value==-1||update_forced_counter==update_forced_loops)){
								adc_last_value=adc_raw_value; //backup value
								if(adc_reverse){alsa_value=nns_map_int(adc_last_value,adc_low_value,adc_high_value,alsa_high_value,alsa_low_value); //compute alsa value, adc reversed
								}else{alsa_value=nns_map_int(adc_last_value,adc_low_value,adc_high_value,alsa_low_value,alsa_high_value);} //compute alsa value
								debug_print("ADC value : %d, ALSA new dB : %.2f\n",adc_raw_value,(float)alsa_value/100);
								alsa_err=snd_mixer_selem_set_playback_volume_all(elem,alsa_value); //set alsa volume
								if(alsa_err<0){debug_print("Warning, failed to update ALSA Mixer volume\n",adc_raw_value,alsa_value);}
							}
						}
					}
				}
				close(i2c_handle);
			}
			
			if(adc_raw_value<0){
				i2c_retry++; //something failed at one point so retry
				if(i2c_retry>2){debug_print("Warning, ADC failed 3 times, skipping until next update\n");}else{sleep(1);}
			}else{i2c_retry=3;} //data read with success, no retry
		}
		
		update_forced_counter++;
		if(update_forced_counter>update_forced_loops){update_forced_counter=0;}
		usleep(update_interval*1000);
	}
	
	if(!test_mode){snd_mixer_close(handle);}
	return(0);
}
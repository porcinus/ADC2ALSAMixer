# ADC2ALSAMixer
The main idea of this program is to read a value from a potentiometer to control the volume output directly from ALSA.  
That way, it limit the additional noise from ground on lower volume.  

# Todo


# History
- 0.1a : Initial release.  

# Provided scripts :
- compile.sh : Compile cpp file (run this first).  
- install.sh : Install service, You may need to edit 'nns-adc2alsamixer-daemon.service'.  
- remove.sh : Remove service.  

# Found needed informations
- alsacard : In most case, it is set to 'default'.  
- alsaname : Run `alsamixer`, and search for 'Item'.  
- i2caddr : Run `sudo i2cdetect -y 1`, MCP3021A/MCP3221A adress range : 48-4F.  
- adcmin/adcmax : Run `./nns-adc2alsamixer-daemon -test` and move the potentiometer to its limit multiple times.  

# Troubleshot
If you are having some troubles, run `./nns-adc2alsamixer-daemon -debug 1`, this will provide additional informations.

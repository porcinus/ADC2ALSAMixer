g++ nns-adc2alsamixer-daemon.cpp -o nns-adc2alsamixer-daemon -lasound
#./nns-adc2alsamixer-daemon -test -i2caddr 0x4D -adcmin 0 -adcmax 4095 -alsamin 0 -alsamax 80
#./nns-adc2alsamixer-daemon -debug 1 -alsaname "PCM" -i2caddr 0x4D -adcmin 1047 -adcmax 2566 -adcreverse -alsamin "-100" -alsamax 0

#!/bin/bash
cp nns-adc2alsamixer-daemon-gbc.service /lib/systemd/system/nns-adc2alsamixer-daemon.service
systemctl enable nns-adc2alsamixer-daemon.service
systemctl start nns-adc2alsamixer-daemon.service
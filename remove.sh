#!/bin/bash
systemctl stop nns-adc2alsamixer-daemon.service
systemctl disable nns-adc2alsamixer-daemon.service
rm /lib/systemd/system/nns-adc2alsamixer-daemon.service
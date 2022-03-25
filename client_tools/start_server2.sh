
#### by using sdk

/usr/bin/killall fmusim_20_cs


cp -f /root/projects/fmusdk-rtai/dist/fmu20/cs/fmusim_20_cs   /tmp


rm -rf /tmp/fmuTmp*


cd /tmp; unzip -o fmu.fmu ; nohup /tmp/fmusim_20_cs /tmp/fmu.fmu  &


/usr/bin/killall copyfile.py

nohup /orthlib/copyfile.py   & 

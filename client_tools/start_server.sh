
mkdir -p /tmp/fmufolder

killall server_tcp

/orthlib/fmu_load_to_tmp /tmp/fmu.fmu>/dev/null 2>&1

cp /tmp/fmufolder/binaries/linux64/*.so /tmp/fmu.so

export FMUNAME=$(cat /tmp/fmufolder/modelDescription.xml   | grep modelIdentifier  | awk -F'=' '{print $2}' |  awk -F'"' '{print $2}')


nohup /root/projects/OrthRTOSServer/build/server_tcp  & 

sleep 1 

/root/projects/OrthRTOSServer/build/client_test localhost

killall server_tcp

Requirements to run/compile omxreceiver

1) Update Raspberry Pi firmware
        sudo rpi-update
        
2) Build libraries
        cd /opt/vc/src/hello_pi
        make -C libs/ilclient
        make -C libs/vgfont

3) Executable can be compiled as follows
        cd projects/omxreceiver
        make
        
4) To run executable
        ./Main.bin <multicast address> <port> <video PID>

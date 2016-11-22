## Hawkeye

Hawkeye is a simple, robust, easy to use USB webcam streaming web server which uses MJPEG as the video codec. It is designed to be usable on local networks as well as the Internet, supporting HTTPS and Basic Authentication. It comes with an HTML video stream viewer optimized for desktop and mobile usage. Lastly, Hawkeye supports multiple webcams.

Hawkeye was built to work on low power devices such as the Raspberry Pi but will work equally well on desktops and laptops alike.

## Installation

If you are running Debian, Ubuntu, Raspbian, or another Debian derivative, you can use my package repository as follows:

    sudo apt-key adv --recv-keys --keyserver keyserver.ubuntu.com 2272781B
    echo "deb http://debs.ridgebit.net/qoSBonHMiqBNAAe5TNm3M0PuZaV91peH/ custom main" | sudo tee /etc/apt/sources.list.d/ridgebit.list
    sudo apt-get update
    sudo apt-get install hawkeye

If you already have a webcam connected, Hawkeye will start listening on localhost, port 8000. Point your browser to http://localhost:8000/ to view the video stream.

If you want to build Hawkeye from source:

    sudo apt-get install build-essential debhelper libv4l-dev libjpeg8-dev libssl-dev git
    git clone https://github.com/ipartola/hawkeye.git
    cd hawkeye/
    make
    sudo make install

If you want to roll your own .deb package:

    sudo apt-get install build-essential debhelper libv4l-dev libjpeg8-dev libssl-dev git devscripts
    git clone https://github.com/ipartola/hawkeye.git
    cd hawkeye/
    debuild
    cd ..

## Configuration

All configuration by default is stored in /etc/hawkeye/hawkeye.conf. The configuration options are self-explanatory (host, port, video sources, SSL settings, etc.) Do note that you should obtain a valid SSL certificate and set it up if you plan on accessing these video streams over the Internet. That is the *only* way to secure your video streams from people snooping on them.

## Still images

In addition to the MJPEG streams, you can get stills from each webcam at /still/NUM. For example: http://localhost:8000/still/0

## Hardware Selection

Hawkeye works with UVC (USB Video Class) devices, and can handle both MJPEG and raw YUV streams. Note that MJPEG is highly recommended as that is what Hawkeye outputs so it requires no transcoding. Hawkeye will log a warning if it is unable to use MJPEG directly from the webcam.

Newer webcams should natively output MJPEG but do check the specs before buying them.

The server was tested to run on a regular Ubuntu desktop, a netbook, and a Raspberry Pi. The Raspberry Pi is the primary target for this project. If used to stream video just over LAN Hawkeye will consume less than 5% of the CPU (assuming one MJPEG-capable webcam input). The Raspberry Pi lacks hardware accelerated encryption so using HTTPS will slow things down: the CPU usage per user will jump by about 10%.

## License

Hawkeye is licensed under GPL-3 unless specified differently in the source files. It also links to the OpenSSL library which adds its own restrictions. See COPYING for details.

## Credits

Much of the video capture code was borrowed and modified from the mjpeg-streamer project. The web UI is based on Bootstrap and jQuery. The rest was written from scratch by Igor Partola <igor@igorpartola.com> (https://igorpartola.com).


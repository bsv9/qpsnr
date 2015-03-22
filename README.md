qpsnr - A quick PSNR/SSIM analyzer for Linux
=====

This is a port for libavcodec api version >= 54.

Original version written by Emanuele Oriani. [http://qpsnr.youlink.org](http://qpsnr.youlink.org)

Build Status
============

[![Build Status](https://drone.io/github.com/MarcAntoine-Arnaud/qpsnr/status.png)](https://drone.io/github.com/MarcAntoine-Arnaud/qpsnr/latest)

Usage
======

    qpsnr v0.2.1 - (C) 2010 E. Oriani
    Usage: qpsnr [options] -r ref.video compare.video1 compare.video2 ...

    -r,--reference:
            set reference video (mandatory)

    -v,--video-size:
            set analysis video size WIDTHxHEIGHT (ie. 1280x720), default is reference video size

    -s,--skip-frames:
            skip n initial frames

    -m,--max-frames:
            set max frames to process before quit

    -I,--save-frames:
            save frames (ppm format)

    -G,--ignore-fps:
            analyze videos even if the expected fps are different

    -a,--analyzer:
            psnr : execute the psnr for each frame
            avg_psnr : take the average of the psnr every n frames (use option "fpa" to set it)
            ssim : execute the ssim (Y colorspace) on the frames divided in blocks (use option "blocksize" to set the size)
            avg_ssim : take the average of the ssim (Y colorspace) every n frames (use option "fpa" to set it)

    -o,--aopts: (specify option1=value1:option2=value2:...)
            fpa : set the frames per average, default 25
            colorspace : set the colorspace ("rgb", "hsi", "ycbcr" or "y"), default "rgb"
            blocksize : set blocksize for ssim analysis, default 8

    -l,--log-level:
            0 : No log
            1 : Errors only
            2 : Warnings and errors
            3 : Info, warnings and errors (default)
            4 : Debug, info, warnings and errors

    -h,--help:
            print this help and exit

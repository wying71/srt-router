# How to run

## Windows
- unzip SrtRouter release package into folder like C:\SrtRouter
- click SrtRouter.exe to run directly

## Linux
- unzip srtrouter release package into folder like /opt/srtrouter
- go into folder and run srtrouter directly


# How to install as Service

## Windows
Please use winsw(https://github.com/winsw/winsw) to wrap it as a Windows service

## Linux
Please use systemd service script to install it as Linux service

# How to test

## Publisher
ffmpeg -re -stream_loop -1 -i C:\Work\Media\bipbop_480x360.ts -c copy -f mpegts "srt://127.0.0.1:8890?streamid=#!::m=publish,r=srt1"

## Subscriber (play)
ffplay "srt://127.0.0.1:8890?streamid=#!::m=request,r=srt1"

# Deadlocked Map Loader (WIP)

Loads USB FS drivers and hooks into Deadlocked's level loader to reroute CDVD requests to USB.

## Credits

 * usbserv: Originally written by jimmikaelkael
 * usbhdfsd: From [PS2SDK](https://github.com/ps2dev/ps2sdk)

## Known Issues

* Only hooks into Local Play level loader
* Freezes when entering and exiting online profile select
* Doesn't support loading minimap from USB
* Doesn't support loading music from USB

## Building


1. You'll want to install the PS2SDK. I recommend using docker.

    ```docker pull ps2dev/ps2dev```

2. Then you'll want to clone the repo:

    ```git clone https://github.com/Dnawrkshp/deadlocked-map-loader.git```

3. cd into the repo

    ```cd deadlocked-map-loader```

4. To run the ps2dev docker image with the repo mounted run:

    ```docker run -it --rm -v "$PWD\:/src" ps2dev/ps2dev```

5. Now in the docker command line add make:

    ```apk add make```

6. Then just cd into the src folder and make

    ```cd src```

    ```make```

## Running

This project outputs a stripped elf that can be loaded into EE memory at 0x000D0000.

To run, you must install a hook that calls the entrypoint at 0x000D0000.

## Future Plans

Running this requires a complex setup using my deadlocked medius server. To make this more accessible I'd like to make a launcher that installs this into the kernel and hooks the IOP reset. 
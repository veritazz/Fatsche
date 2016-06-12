# Fatsche

Arduboy game

The directory *graphics* contains a script that converts all aseprite files
into C code when run with the *-a* option. These files are not uploaded so run
the script without this option in order to convert the *.png* files to C code.

```
cd graphics
./convert_graphics.sh
```

# Installation

Install platformio as described here

http://docs.platformio.org/en/latest/installation.html#installation-methods

plugin your Arduboy and run:

```
platformio run --target upload
```

If this is not working then push the reset button of the Arduboy and try again.
Alternatively hold the **UP** key then switch on the Arduboy. Now you can run
the platformio command from above.

Enjoy

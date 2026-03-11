fontconvert
============

Prerequisite you need to install python3 and install `freetype-py` using pip
The approximate process is like this:

.. code-block:: shell

    sudo apt install python3-pip
    python3 -m pip install freetype-py

The previous is the prerequisite for implementation, and then you need to store the font file you want to convert in the same path as fontconvert.py. This is just for more convenient operation. You can also fill in the font path.

Then just follow the command below to convert the font.

.. code-block:: shell

    python3 fontconvert.py --compress demo 16 msyh.ttc > font.h

Explanation of specific parameters:

.. code-block:: shell

    python3 fontconvert.py --compress [generated font name] [font size] [font file path] > [generated font file]

Of course, this only demonstrates the generation of standard ascii codes. If you need other fonts, you only need to fill in the unicode encoding of the font to be generated in the `fontconvert.py`  `intervals` list.

Please make sure that the unicode encoding in the fontconvert.py intervals list is included in your font file, otherwise please comment other encodings and only keep the 32,126 range!


Image Conversion (6-color EPD)
==============================

This project uses a 6-color EPD palette for image rendering.

Palette mapping used by firmware:

- black  -> `0x0`
- white  -> `0x1`
- yellow -> `0x2`
- red    -> `0x3`
- blue   -> `0x5`
- green  -> `0x6`


1) Convert image to 6-color BMP
-------------------------------

Use the existing converter in `scripts/ConvertTo6C`.

Example:

.. code-block:: shell

    python3 ConvertTo6C/convert.py input.jpg --mode scale --dither 3

This creates an output BMP next to the input, for example:

.. code-block:: text

    input_scale_output.bmp


2) Convert 6-color BMP to C++ header
------------------------------------

Use `bmp6_to_cpp_header.py` to generate a header compatible with the firmware fallback image format.

Prerequisite:

.. code-block:: shell

    python3 -m pip install pillow

Basic usage:

.. code-block:: shell

    python3 bmp6_to_cpp_header.py path/to/input_scale_output.bmp

Default output path:

.. code-block:: text

    src/Images/DefaultImage.h

Custom output and array name:

.. code-block:: shell

    python3 bmp6_to_cpp_header.py path/to/input.bmp --output ../src/Images/DefaultImage.h --array-name DefaultImage

Strict mode (default):

- Fails if a pixel is not one of the 6 palette colors.

Non-strict mode:

.. code-block:: shell

    python3 bmp6_to_cpp_header.py path/to/input.bmp --no-strict

- Non-palette colors are mapped to nearest EPD color.


3) Fallback behavior in firmware
--------------------------------

When no image is found on SD/LittleFS, firmware loads the built-in fallback image from:

.. code-block:: text

    src/Images/DefaultImage.h

The header must contain:

- `DefaultImageWidth`
- `DefaultImageHeight`
- `DefaultImage[]` packed as 2 pixels per byte (high nibble = even X, low nibble = odd X)

compressor: compressor.c
	cc -m32 -I$$XMOS_TOOL_PATH/include -O3 compressor.c $$XMOS_TOOL_PATH/lib/libxsidevice.so -o compressor

decompress.xe: image_n0c0.bin compressor
	./compressor image_n0c0.bin -t 0x18000 -o decompress.xe -target=XK-1

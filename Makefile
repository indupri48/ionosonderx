ionosonde_rx:
	gcc -o build/ionosonderx ionosonderx.c matched_filter.c -lliquid -lm -lportaudio

clean:
	rm build/ionosonderx
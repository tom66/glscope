all:
	python3 setup.py build

install:
	python3 setup.py install

clean:
	-rm -rf build/ *.o

run:
	(cd ~/raspiraw;sudo ./camera_i2c)
	sudo chmod 666 /dev/i2c-0
	sudo python3 app.py
	xset s off

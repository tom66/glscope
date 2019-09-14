all:
	python setup.py build

install:
	python setup.py install

clean:
	-rm -rf build/ *.o

run:
	(cd ~/raspiraw;sudo ./camera_i2c)
	sudo chmod 666 /dev/i2c-0
	sudo python app.py
	xset s off

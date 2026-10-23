
all: build

cmake::
	test -d build || mkdir build
	cd build; cmake ..

build::
	cd build; make

clean:
	cd build; make clean

clobber:
	rm -fr build

# invoke the cmake generated flash target
flash:
	cd build; make flash

# start OpenOCD for gdb access
ocd:
	openocd -f board/ti_lp_em_cc2340r53.cfg

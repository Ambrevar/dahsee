################################################################################
# Dahsee makefile
# Date 2012-06-28
################################################################################

all: 
	@make -C src/

clean:
	@make -C src/ clean
	@make -C test/ clean


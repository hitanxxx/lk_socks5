
install_path := /usr/local/s5
all:
	@make -C ./src
	@echo "=========================="
	@echo "===== compile finish ====="
	@echo "=========================="
	#copy elf file into env dir
	-cp -rf ./src/bin/s5 ./env/sbin/
test:
	@make -C ./src test
	
install:
	#create dir
	-test -d $(install_path) || mkdir -p $(install_path)

	#copy env into dir
	-cp -rf ./env/* $(install_path) 
	
	#create logs dir in install_path
	-test -d $(install_path)/logs || mkdir -p $(install_path)/logs
clean:
	@make -C ./src clean

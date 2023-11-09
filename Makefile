
install_path := /usr/local/s5
all:
	@make -C ./src
	@echo "=========================="
	@echo "===== compile finish ====="
	@echo "=========================="
	#copy elf file into env dir
	-cp -rf ./src/s5 ./env/sbin/
test:
	@make -C ./src test
	
install:
	#create dir
	-test -d $(install_path) || mkdir -p $(install_path)

	#copy env into dir
	-cp -rf ./env/* $(install_path) 
	
clean:
	@make -C ./src clean

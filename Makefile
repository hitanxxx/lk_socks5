
install_path := /usr/local/lks5
all:
	@make -C ./src
	@echo "===== compile finish ====="
	-cp -rf ./src/bin/lks5 ./env/sbin/
	-cp -rf ./config/* ./env/config	
test:
	@make -C ./src test
	
install:
	-test -d $(install_path) || mkdir -p $(install_path)
	-cp -rf ./env/* $(install_path) 
clean:
	@make -C ./src clean

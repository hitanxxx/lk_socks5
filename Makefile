
install_path := /usr/local/lks5/

all:
	@make -C ./src 

test:
	@make -C ./src test
	
install:
	-test -d $(install_path) 			|| mkdir -p $(install_path)
	-test -d $(install_path)config 		|| mkdir -p $(install_path)config
	-test -d $(install_path)log 		|| mkdir -p $(install_path)logs
	-test -d $(install_path)sbin 		|| mkdir -p $(install_path)sbin
	
	-cp -rf src/bin/lks5 				$(install_path)sbin
	-cp -rf certificate 				$(install_path)
	-cp -rf www     					$(install_path)

server:all install
	-cp -rf config/config.json.server 	$(install_path)config/config.json
	-cp -rf config/auth.json 			$(install_path)config

client:all install
	-cp -rf config/config.json.client 	$(install_path)config/config.json

clean:
	@make -C ./src clean

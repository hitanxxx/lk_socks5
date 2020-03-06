
all:
	@make -C ./src 

test:
	@make -C ./src test
	
install:
	-test -d /usr/local/lks5 || mkdir -p /usr/local/lks5
	-test -d /usr/local/lks5/config || mkdir -p /usr/local/lks5/config
	-test -d /usr/local/lks5/log || mkdir -p /usr/local/lks5/logs
	-test -d /usr/local/lks5/sbin || mkdir -p /usr/local/lks5/sbin
	-cp -rf src/bin/lks5 /usr/local/lks5/sbin
	-cp -rf certificate /usr/local/lks5

server:all install
	-cp -rf config/config.json.server /usr/local/lks5/config/config.json
	-cp -rf config/auth.json /usr/local/lks5/config

client:all install
	-cp -rf config/config.json.client /usr/local/lks5/config/config.json

clean:
	@make -C ./src clean

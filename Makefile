all: rmq reset master client atm

master:
	cc master.c -o master

reset:
	reset

run:
	./master

rmq:
	ipcrm -a

atm:
	cc atm.c -o atm

client:
	cc client.c -o client

clean: clean_master clean_atm clean_client

clean_master:
	rm master

clean_atm:
	rm atm

clean_client:
	rm client

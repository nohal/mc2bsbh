all: mc2bsbh

mc2bsbh:
	g++ -s -O3 mc2bsbh.cpp -o mc2bsbh

install: mc2bsbh
	cp mc2bsbh /usr/local/bin

clean:
	rm mc2bsbh

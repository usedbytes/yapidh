package main

import (
	"bytes"
	"encoding/binary"
	"math/rand"
	"net"
	"time"
)

func main() {
	c,err := net.Dial("unix", "/tmp/sock")
	if err != nil {
		panic(err.Error())
	}

	channel := byte(0)
	speed := byte(12)

	for {
		buf := new(bytes.Buffer)

		binary.Write(buf, binary.LittleEndian, uint32(1));
		binary.Write(buf, binary.LittleEndian, uint32(2));
		binary.Write(buf, binary.LittleEndian, channel);
		binary.Write(buf, binary.LittleEndian, speed);

		_,err := c.Write(buf.Bytes())
		if err != nil {
			println(err.Error())
		}
		time.Sleep(1500 * time.Millisecond)
		speed = byte(rand.Intn(25)) + 1
	}
}

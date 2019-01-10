// TODO: Use proper testing framework

package main

import (
	"bytes"
	"context"
	"io"
	"fmt"
	"log"

	"osp"
)

func testMdns() {
	// TODO: log error if it fails
	ctx := context.Background()
	go osp.RunMdnsServer(ctx, "TV", 10000)
	entries, err := osp.BrowseMdns(ctx)
	if (err != nil) {
		log.Fatalf("Failed to browse mDNS: %v\n", err)
	}
	for entry := range entries {
		log.Println(fmt.Sprintf("%s at %s(%s or %s):%d", entry.Instance, entry.HostName, entry.AddrIPv4, entry.AddrIPv6, entry.Port))
	}
	log.Println("No more entries.")
}

func testQuic() {
	port := 4242
	msg := []byte("Hello, World")
	tlsCert, err := osp.GenerateTlsCert()
	if err != nil {
		log.Fatalln("Failed to generate TLS cert:", err.Error())
	}

	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	go func() {
		clientSession, err := osp.DialAsQuicClient(ctx, "localhost", port)
		if err != nil {
			log.Fatalln("Failed to dial QUIC:", err.Error())
		}
		clientStream, err := clientSession.OpenStreamSync()
		if err != nil {
			log.Fatalln("Failed to open QUIC stream:", err.Error())
		}
		_, err = clientStream.Write(msg)
		if err != nil {
			log.Fatalln("Failed to write to a QUIC stream:", err.Error())
		}
	}()


	serverStreams := make(chan io.ReadWriteCloser)
	go osp.RunQuicServer(ctx, port, *tlsCert, serverStreams)
	serverStream := <-serverStreams
	b := make([]byte, len(msg))
	_, err = io.ReadFull(serverStream, b)
	if err != nil {
		log.Fatalln("Failed to read stream:", err.Error())
	}
	log.Println(string(b))
}

func testMessages() {
	msg := osp.PresentationStartRequest{
		RequestID:      1,
		PresentationID: "This is a Presentation ID.  It must be very long.",
		URL:            "https://github.com/webscreens/openscreenprotocol",
	}

	var rw bytes.Buffer
	osp.WriteMessage(msg, &rw)
	msg2, err := osp.ReadMessage(&rw)
	if err != nil {
		log.Fatalln("Failed to read message.");
	}
	log.Println(msg2)
}

func testVarint() {
	n1 := uint64(12345)
	rw := bytes.NewBuffer([]byte{})
	log.Printf("Write: %d\n", n1)
	err := osp.WriteVaruint(n1, rw)
	if err != nil {
		log.Fatalln("Failed to write varint", err.Error())
	}
	n2, err := osp.ReadVaruint(rw)
	if err != nil {
		log.Fatalln("Failed to read varint", err.Error())
	}
	log.Printf("Read: %d\n", n2)
}

func main() {
	testVarint()
	testMessages()
	testQuic()
	testMdns()
}

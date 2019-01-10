package main

// TODO: 
//  Add response messages from receiver

//  Inject JS into viewURL to using .Eval and .Bind to send and receiver presentation connection messages

import (
	"context"

	"flag"
	"fmt"
	"log"

  "osp"

	"github.com/zserge/webview"
)

func viewUrl(url string) {
	web := webview.New(webview.Settings{
		// Title: "Open Screen",
		URL:       url,
		Width:     800,
		Height:    600,
		Resizable: true,
	})
	web.Dispatch(func() {
		// web.SetFullscreen(true)
	})
	web.Dispatch(func() {
		// web.Eval();
		// web.Bind();
	})
	web.Run()
}

func main() {
	port := flag.Int("port", 10000, "port")
	flag.Parse()
	args := flag.Args()
	if len(args) < 1 {
		log.Fatalln("Usage: osp server name; osp browse; osp fling url")
	}
	
	ctx := context.Background()

	cmd := args[0]
	switch cmd {
	case "server":
		if len(args) < 2 {
			log.Fatalln("Usage: osp server name")
		}
		mdnsInstanceName := args[1]

		tlsCert, err := osp.GenerateTlsCert()
		if err != nil {
			log.Fatalln("Failed to generate TLS cert:", err.Error())
		}
		osp.RunPresentationReceiver(ctx, mdnsInstanceName, *port, *tlsCert, viewUrl)

	case "browse":
		entries, err := osp.BrowseMdns(ctx)
		if (err != nil) {
			log.Fatalf("Failed to browse mDNS: %v\n", err)
		}
	  for entry := range entries {
			log.Println(fmt.Sprintf("%s at %s(%s or %s):%d", entry.Instance, entry.HostName, entry.AddrIPv4, entry.AddrIPv6, entry.Port))
		}


	case "fling":
		if len(args) < 3 {
			log.Fatalln("Usage: osp fling target url")
		}
		target := args[1]
		url := args[2]

		log.Printf("Search for %s\n", target)
	  entries, err := osp.BrowseMdns(ctx)
		if (err != nil) {
			log.Fatalf("Failed to browse mDNS: %v\n", err)
		} 
		for entry := range entries {
			if entry.Instance == target {
				log.Printf("Fling %s to %s:%d\n", url, entry.HostName, entry.Port)
				err := osp.StartPresentation(ctx, entry.HostName, entry.Port, url);
				if err != nil {
					log.Fatalln("Failed to start presentation.");
				}
				break
			}
		}

	case "view":
		if len(args) < 2 {
			log.Fatalln("Usage: osp view url")
		}

		url := args[1]

		viewUrl(url)
	}
}

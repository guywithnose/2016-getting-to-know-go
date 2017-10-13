package main

import (
	"fmt"
	"log"
	"os"
)

func main() {
	file, err := os.Open("slides")
	if err != nil {
		log.Fatal(err)
	}

	info, err := file.Stat()
	if err != nil {
		log.Fatal(err)
	}

	fmt.Println(info.Size())
}

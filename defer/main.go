package main

import (
	"fmt"
	"log"
	"os"
)

func main() {
	size, err := getFileSize("slides")
	if err != nil {
		log.Fatal(err)
	}
	fmt.Println(size)
}
func getFileSize(fileName string) (int64, error) {
	file, err := os.Open(fileName)
	if err != nil {
		return 0, err
	}
	defer file.Close()
	info, err := file.Stat()
	if err != nil {
		return 0, err
	}

	return info.Size(), nil
}

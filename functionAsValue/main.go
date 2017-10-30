package main

import "fmt"

func main() {
	name := "Bob"
	sayHello := func(name string) string {
		return fmt.Sprintf("Hello, %s!\n", name)
	}

	fmt.Printf("Hello, %s!\n", name)
	runFunction(name, sayHello)
}

func runFunction(name string, f func(string) string) string {
	return f(name)
}

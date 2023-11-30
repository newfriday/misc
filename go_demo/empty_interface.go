package main

import "fmt"

func main() {
	var a interface{}
	fmt.Printf("a == nil is %t\n", a == nil)

	a = 1

	fmt.Printf("a = %d\n", a)

	a = nil
	fmt.Printf("a == nil is %t\n", a == nil)
}

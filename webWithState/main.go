package main

import (
	"database/sql"
	"fmt"
	"html/template"
	"log"
	"net/http"

	_ "github.com/mattn/go-sqlite3"
)

type dataHolder struct {
	Message string
	Values  []string
}

var tmpl *template.Template
var db *sql.DB

func main() {
	var err error
	tmpl, err = template.ParseFiles("templates/main.tmpl")
	if err != nil {
		log.Fatal(err)
	}

	db, err = sql.Open("sqlite3", "./data.db")
	if err != nil {
		log.Fatal(err)
	}

	http.Handle("/assets/", http.StripPrefix("/assets/", http.FileServer(http.Dir("./public"))))
	http.HandleFunc("/", rootHandler)
	http.HandleFunc("/reset", resetHandler)
	err = http.ListenAndServe(":8082", nil)
	if err != nil {
		log.Fatal(err)
	}
}

func rootHandler(w http.ResponseWriter, r *http.Request) {
	if r.Method == "POST" {
		postHandler(w, r)
		return
	}

	getHandler(w, r)
}

func getHandler(w http.ResponseWriter, r *http.Request) {
	message := ""
	cookie, err := r.Cookie("message")
	if err == nil {
		message = cookie.Value
		http.SetCookie(w, &http.Cookie{Name: "message", MaxAge: -1})
	}

	var values = make([]string, 0)
	result, err := db.Query(`select value from data`)
	if err != nil {
		err = tmpl.Execute(w, dataHolder{Message: message})
		if err != nil {
			fmt.Fprintln(w, err)
			return
		}
		return
	}

	for result.Next() {
		var value string
		err = result.Scan(&value)
		if err != nil {
			fmt.Fprintln(w, err)
			return
		}

		values = append(values, value)
	}

	err = tmpl.Execute(w, dataHolder{Values: values, Message: message})
	if err != nil {
		fmt.Fprintln(w, err)
		return
	}
}

func postHandler(w http.ResponseWriter, r *http.Request) {
	_, err := db.Exec(`insert into data (value) values (?)`, r.FormValue("data"))
	if err != nil {
		fmt.Fprintln(w, err)
		return
	}

	getHandler(w, r)
}

func resetHandler(w http.ResponseWriter, r *http.Request) {
	_, err := db.Exec(`delete from data`, r.FormValue("data"))
	if err != nil {
		_, createError := db.Exec(`create table data (value text)`)
		if createError != nil {
			fmt.Fprintln(w, err)
			fmt.Fprintln(w, createError)
			return
		}
	}

	http.SetCookie(w, &http.Cookie{Name: "message", Value: "Database reset"})
	http.Redirect(w, r, "/", 302)
}

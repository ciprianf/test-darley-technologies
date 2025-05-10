

## P1.

Self-evaluation of rust: 1.

I did once an introductory course to Rust, but I am eager to learn. Given my
background in C++, the transition should not be difficult.


## P2

Download the dictionary,curate it and extract the words:

```bash

wget https://www.gutenberg.org/files/98/98-0.txt

# Read all text, do some input cleanup and extract all words (lower-case)
cat 98-0.txt | sed -e "s/‘/'/g" -e "s/’/'/g" -e 's/“/"/g' -e 's/”/"/g' | tr -s '[:space:][:punct:][:digit:]"' '\n'| tr '[:upper:]' '[:lower:]' |  sort | uniq > words.txt
```

Total number of words: 19878
Max length of a word is 17 (for word "undistinguishable").

Code: p2.cpp


## P3

The problem asks us to implement a JSON parse for a specific endpoint. A generic JSON parser can be quite complex, but
because this specific endpoints returns a certain schema, we can optimize for it.

Documentation for ticker at: https://developers.binance.com/docs/derivatives/option/market-data/24hr-Ticker-Price-Change-Statistics

Download ticker data:
```
$ wget -O ticker.json https://eapi.binance.com/eapi/v1/ticker
```

We can measure the parsing speed of the program using a utility like time. For the `ticker.json` input, we get on my machine

```
real	0m0.088s
user	0m0.080s
sys	    0m0.000s
```

For more accurate measurements, we should use some better tooling. Process isolation and multiple iterations of the same test
would give more meaningful numbers.

Code: p3.cpp
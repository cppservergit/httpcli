# httpcli
Modern C++ 23 wrapper for libcurl, C++ core guidelines compliant, tested with GNU G++ memory and thread sanitizers on Ubuntu 24.04.

Baseline code is 100% compliant with Sonar Clound analysis:
![image](https://github.com/user-attachments/assets/eb98c2c5-0a32-4a7b-9e76-4742387e61b0)

![image](https://github.com/user-attachments/assets/a1e3a1f4-49ee-48c2-9d65-d962df8d9bef)

Support for ignoring invalid certificates, which is useful in development environments, can be added, but doing so would break the "A" score achieved by the baseline code.

Some directives to ignore code were added because:
1) SonarCloud can't understand that libcurl write callbacks require a void* parameter; this cannot be changed.
2) A SonarCloud bug raises the issue of an unsafe protocol despite the clear use of TLS 1.2 as the minimum accepted.

Check `main.cpp` for examples and unit tests.

To compile and create the static library
```
make
```

To create the test executable and the library if necessary
```
make test
```

Run
```
test
```

Tested on Windows 10 with MSYS2 packages and Ubuntu 24.04.

# httpcli
Modern C++ 23 wrapper for libcurl, core guidelines compliant

Baseline code is 100% compliant with Sonar Clound analysis:
![image](https://github.com/user-attachments/assets/eb98c2c5-0a32-4a7b-9e76-4742387e61b0)

Support for ignoring invalid certificates, which is useful in development environments, can be added but would break the "A" score achieved by the baseline code.

Some directives to ignore code were added because:
1) SonarCloud can't understand that libcurl write callbacks require a void* parameter; this cannot be changed.
2) A SonarCloud bug raises the issue of an unsafe protocol despite the clear use of TLS 1.2 as the minimum accepted.


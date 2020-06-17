#ifndef PASSWORD_H
#define PASSWORD_H

int password_hash(char *psw_hashed, const char *psw, const char *salt);

#endif  /* PASSWORD_H */

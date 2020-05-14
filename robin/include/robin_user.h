/*
 * robin_user.h
 *
 * Header file containing the public interface for accessing Robin Users data
 * on memory and file system.
 */

#ifndef ROBIN_USER_H
#define ROBIN_USER_H

#define ROBIN_USER_EMAIL_LEN 64
#define ROBIN_USER_PSW_LEN   64

typedef struct robin_user_data {
    /* Login information */
    char email[ROBIN_USER_EMAIL_LEN];
    char psw[ROBIN_USER_PSW_LEN];  /* hashed password */

    /* Data release information */
    int uid;
} robin_user_data_t;

/**
 * @brief Acquire (exclusive access) the user data if email:psw are valid
 *
 * @param email user email
 * @param psw   user password (hashed)
 * @param data  return argument, the pointer will be set to allocated user data
 * @return int  0 on success
 *             -1 on error
 *              1 user data already acquired
 *              2 invalid email or password
 */
int robin_user_acquire_data(const char *email, const char *psw, robin_user_data_t **);

/**
 * @brief Release (exclusive access) the user data
 *
 * @param data user data
 */
void robin_user_release_data(robin_user_data_t *data);

/**
 * @brief Add the user identified by email:psw to the system
 *
 * @param email user email
 * @param psw   user password (hashed)
 * @return int  0 on success
 *             -1 on error
 *              1 on invalid email or password
 *              2 on email already used
 */
int robin_user_add(const char *email, const char *psw);

#endif /* ROBIN_USER_H */
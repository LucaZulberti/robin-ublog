/*
 * robin_user.h
 *
 * Header file containing the public interface for accessing Robin Users data
 * on memory and file system.
 */

#ifndef ROBIN_USER_H
#define ROBIN_USER_H

/**
 * @brief Acquire (exclusive access) the user if email:psw are valid
 *
 * @param email user email
 * @param psw   user password (hashed)
 * @param user  return argument, the uid
 * @return int  0 on success
 *             -1 on error
 *              1 user data already acquired
 *              2 invalid email or password
 */
int robin_user_acquire(const char *email, const char *psw, int *user);

/**
 * @brief Release (exclusive access) the user
 *
 * @param user user to release
 */
void robin_user_release(int uid);

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

/**
 * @brief Get the email of the user
 *
 * @param user         the user id
 * @return const char* its email
 */
const char *robin_user_email_get(int uid);

#endif /* ROBIN_USER_H */
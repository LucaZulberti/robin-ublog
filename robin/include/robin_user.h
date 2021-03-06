/*
 * robin_user.h
 *
 * Header file containing the public interface for accessing Robin Users data
 * on memory and file system.
 */

#ifndef ROBIN_USER_H
#define ROBIN_USER_H

#include "robin.h"

/**
 * @brief Load users and password from file in memory
 *
 * @param filename path to user file
 * @return int     0 on success; -1 on error
 */
int robin_users_load(const char *filename);

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

/**
 * @brief Get a vector with followed users
 *
 * The returned pointer 'following' must be freed by the caller.
 *
 * @param uid       the user id
 * @param following the vector of emails (return)
 * @param len       the vector len (return)
 * @return int      0 on success
 *                 -1 on error
 */
int robin_user_following_get(int uid, char ***following, size_t *len);

/**
 * @brief Get a vector of followers
 *
 * The returned pointer 'followers' must be freed by the caller.
 *
 * @param uid       the user id
 * @param followers the vector of emails (return)
 * @param len       the vector len (return)
 * @return int      0 on success
 *                 -1 on error
 */
int robin_user_followers_get(int uid, char ***followers, size_t *len);

/**
 * @brief Make the user follow the one identified by email
 *
 * @param uid   the user id
 * @param email email of the user that will be followed
 * @return int  0 on success
 *             -1 on error
 *              1 on user identified by email does not exist
 *              2 on user identified by email already followed
 */
int robin_user_follow(int uid, const char *email);

/**
 * @brief Make the user unfollow the one identified by email
 *
 * @param uid   the user id
 * @param email email of the user that will be followed
 * @return int  0 on success
 *             -1 on error
 *              1 on user identified by email is not followed
 */
int robin_user_unfollow(int uid, const char *email);

/**
 * @brief Free up the resources to terminate gracefully
 *
 * The resources of acquired users are not freed, you must release any
 * acquired user by yourself before calling this function.
 */
void robin_user_free_all(void);

#endif /* ROBIN_USER_H */

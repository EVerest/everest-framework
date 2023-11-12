// Creates pipe in a cross-platform way.
// params:
//     pipefd: pointer to an array of two integers, where file descriptors for
//             the pipe will be stored.
void create_pipe(int* pipefd);
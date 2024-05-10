#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <sys/wait.h>
#include <libgen.h>
#include <fcntl.h>

#define MAX_PERMISSIONS S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP | S_IROTH | S_IWOTH | S_IXOTH
#define MAX_NUMBER_OF_DIRECTORIES 10
#define MAX_DIRECTORY_NAME 50
#define MAX_FILE_NAME 100
#define MAX_MODIFIED_LENGTH 100
#define MAX_PERMISSIONS_LENGTH 50
#define MAX_PATH_LENGTH 100
#define BUFFER_SIZE 512

typedef struct
{

    char name[MAX_FILE_NAME];
    off_t size;
    time_t modified; // Time of last modification //
    ino_t inode;
    mode_t mode;                            // File type and mode //
    char modifiedChar[MAX_MODIFIED_LENGTH]; // the modified field in human-readable format
    char permissions[MAX_PERMISSIONS_LENGTH];
    char path[MAX_PATH_LENGTH];
} MetaData;

int checkDirectories(char **argv, int argc, int start, char dirNames[MAX_NUMBER_OF_DIRECTORIES][MAX_DIRECTORY_NAME] , char izolated_space_dir[MAX_DIRECTORY_NAME] , char output_dir[MAX_DIRECTORY_NAME]); // Check if the directories given resepect the requirements, puts their names in dirNames and returns the number of directories
void readDir(char *path, char *output_dir, char *izolated_space_dir, int *dangerous_files);                           // Opens the directory and reads all the files
MetaData makeMetaData(char *path, struct dirent *dirData);                                                            // Returns a MetaData from the file's path given
void makeSnapshot(char *path, MetaData metadata, char *output_dir,ino_t inode,int index);                                                   // Makes the snapshot file or updates the already existing one if there are changes
MetaData parseMetaDataFromFile(const char *file_path,int index);                                                                // Returns a MetaData from a given file
int compareMetaData(MetaData new, MetaData old);                                                                      // Compares two metadatas
void printMetaData(MetaData metadata, int fd);                                                                        // Prints the metadata in the given file, if there is none given it will print to stdout
char *permissionToString(mode_t mode);                                                                                // Returns a human-readable string with the file permissions
int analyze_file(char *pathCurrent, char *izolated_space_dir);                                                        // Executes the script and moves the file if it is dangerous
int verifyType(char *DirectoryName);                                                                                  // Check if the argument is a directory


void printMetaData(MetaData metadata, int fd)
{
    char buffer[BUFFER_SIZE]; // Buffer to hold formatted strings
    int n;                    // Variable to store number of bytes written

    n = snprintf(buffer, sizeof(buffer), "Path: %s\n", metadata.path);
    if (write(fd, buffer, n) == -1)
    {
        perror("Write failed\n");
        exit(-6);
    }

    n = snprintf(buffer, sizeof(buffer), "Name: %s\n", metadata.name);
    if (write(fd, buffer, n) == -1)
    {
        perror("Write failed\n");
        exit(-6);
    }

    n = snprintf(buffer, sizeof(buffer), "Size: %ld bytes\n", metadata.size);
    if (write(fd, buffer, n) == -1)
    {
        perror("Write failed\n");
        exit(-6);
    }

    n = snprintf(buffer, sizeof(buffer), "Last Modified: %s\n", metadata.modifiedChar);
    if (write(fd, buffer, n) == -1)
    {
        perror("Write failed\n");
        exit(-6);
    }

    n = snprintf(buffer, sizeof(buffer), "Inode: %lu\n", metadata.inode);
    if (write(fd, buffer, n) == -1)
    {
        perror("Write failed\n");
        exit(-6);
    }

    n = snprintf(buffer, sizeof(buffer), "Permissions: %s\n", metadata.permissions);
    if (write(fd, buffer, n) == -1)
    {
        perror("Write failed\n");
        exit(-6);
    }
}

char *permissionToString(mode_t mode)
{
    static char perm[10];
    perm[0] = (mode & S_IRUSR) ? 'r' : '-';
    perm[1] = (mode & S_IWUSR) ? 'w' : '-';
    perm[2] = (mode & S_IXUSR) ? 'x' : '-';
    perm[3] = (mode & S_IRGRP) ? 'r' : '-';
    perm[4] = (mode & S_IWGRP) ? 'w' : '-';
    perm[5] = (mode & S_IXGRP) ? 'x' : '-';
    perm[6] = (mode & S_IROTH) ? 'r' : '-';
    perm[7] = (mode & S_IWOTH) ? 'w' : '-';
    perm[8] = (mode & S_IXOTH) ? 'x' : '-';
    perm[9] = '\0';
    return perm;
}

MetaData makeMetaData(char *path, struct dirent *dirData)
{
    struct stat statData;
    int data = lstat(path, &statData);
    if (data == -1)
    {
        perror("Stat failed\n");
        exit(-3);
    }

    MetaData metadata;

    strcpy(metadata.name, dirData->d_name);
    metadata.size = statData.st_size;
    metadata.modified = statData.st_mtime;
    metadata.inode = statData.st_ino;
    metadata.mode = statData.st_mode;

    struct tm *tm_info;
    tm_info = localtime(&statData.st_mtime);
    char modified_time_str[26];
    strftime(modified_time_str, 26, "%Y-%m-%d %H:%M:%S", tm_info);

    strcpy(metadata.modifiedChar, modified_time_str);

    mode_t permissions = statData.st_mode & 0777;
    char *permission_str = permissionToString(permissions);
    strcpy(metadata.permissions, permission_str);

    strcpy(metadata.path, path);

    return metadata;
}

ssize_t read_line(int fd, char *buffer, size_t n) {
    ssize_t num_read;  // numărul de octeți citiți de read
    size_t total_read = 0;  // numărul total de octeți citiți
    char ch;

    if (n <= 0 || buffer == NULL) {
        return -1;
    }

    while (total_read < n - 1) {  // -1 pentru a lăsa loc pentru terminatorul de șir
        num_read = read(fd, &ch, 1);  // citim un singur caracter

        if (num_read == 1) {  // am citit cu succes un caracter
            if (ch == '\n') {  // dacă am întâlnit un newline, ieșim din buclă
                break;
            }
            buffer[total_read++] = ch;  // adăugăm caracterul în buffer
        } else if (num_read == 0) {  // am ajuns la EOF
            break;
        } else {  // eroare la citire
            return -1;
        }
    }

    buffer[total_read] = '\0';  // adăugăm terminatorul de șir
    return total_read;  // returnăm numărul de caractere citite
}

void moveCursorToNthPath(int fd, int N) {
    char buffer[BUFFER_SIZE]; // Buffer pentru citirea din fișier
    char *word;
    int bytesRead;
    int count = 0;
    off_t position;
    
    // Salvează poziția curentă a cursorului
    position = lseek(fd, 0, SEEK_CUR);
    
    // Întoarce cursorul la începutul fișierului
    lseek(fd, 0, SEEK_SET);
    
    while ((bytesRead = read(fd, buffer, sizeof(buffer))) > 0) {
        // Parcurge bufferul pentru a găsi aparițiile cuvântului "Path"
        word = strtok(buffer, " \t\n"); // Separa cuvintele după spații, tab-uri și newline-uri
        
        while (word != NULL) {
            if (strcmp(word, "Path") == 0) {
                count++;
                if (count == N) {
                    // Găsit cea de-a N-a apariție a cuvântului "Path", mută cursorul la începutul cuvântului
                    lseek(fd, -(strlen(word) + 1), SEEK_CUR); // Se adaugă 1 pentru a compensa spațiul/tabul/newline-ul
                    return;
                }
            }
            word = strtok(NULL, " \t\n");
        }
    }
    
    if (count < N) {
        // Dacă nu au fost găsite suficiente apariții ale cuvântului "Path", se revine la poziția inițială
        lseek(fd, position, SEEK_SET);
    }
}

void printMetaData_index(MetaData metadata, int fd,int index)
{
    moveCursorToNthPath(fd,index);
    char buffer[BUFFER_SIZE]; // Buffer to hold formatted strings
    int n;                    // Variable to store number of bytes written

    n = snprintf(buffer, sizeof(buffer), "Path: %s\n", metadata.path);
    if (write(fd, buffer, n) == -1)
    {
        perror("Write failed\n");
        exit(-6);
    }

    n = snprintf(buffer, sizeof(buffer), "Name: %s\n", metadata.name);
    if (write(fd, buffer, n) == -1)
    {
        perror("Write failed\n");
        exit(-6);
    }

    n = snprintf(buffer, sizeof(buffer), "Size: %ld bytes\n", metadata.size);
    if (write(fd, buffer, n) == -1)
    {
        perror("Write failed\n");
        exit(-6);
    }

    n = snprintf(buffer, sizeof(buffer), "Last Modified: %s\n", metadata.modifiedChar);
    if (write(fd, buffer, n) == -1)
    {
        perror("Write failed\n");
        exit(-6);
    }

    n = snprintf(buffer, sizeof(buffer), "Inode: %lu\n", metadata.inode);
    if (write(fd, buffer, n) == -1)
    {
        perror("Write failed\n");
        exit(-6);
    }

    n = snprintf(buffer, sizeof(buffer), "Permissions: %s\n", metadata.permissions);
    if (write(fd, buffer, n) == -1)
    {
        perror("Write failed\n");
        exit(-6);
    }
}

MetaData parseMetaDataFromFile(const char *file_path, int index)
{
    MetaData metadata;
    int file_descriptor = open(file_path, O_RDONLY);
    if (file_descriptor == -1)
    {
        perror("open");
        exit(-5);
    }
    
    moveCursorToNthPath(file_descriptor,index);

    char line[BUFFER_SIZE];
    ssize_t bytes_read;
    int ok = 0;
    while ((bytes_read = read_line(file_descriptor, line, BUFFER_SIZE) > 0) && ok == 0) // Reads a line from the file
    {
        // check each line for the relevant data
        if (strncmp(line, "Name:", 5) == 0)
        {
            sscanf(line, "Name: %[^\n]", metadata.name);
        }
        else if (strncmp(line, "Size:", 5) == 0)
        {
            sscanf(line, "Size: %ld bytes", &metadata.size);
        }
        else if (strncmp(line, "Inode:", 6) == 0)
        {
            sscanf(line, "Inode: %lu", &metadata.inode);
        }
        else if (strncmp(line, "Permissions:", 12) == 0)
        {
            sscanf(line, "Permissions: %[^\n]", metadata.permissions);
            ok = 1;
        }
    }

    strcpy(metadata.modifiedChar, "x"); // I don't care for this when i compare the two metadas
    strcpy(metadata.path, file_path);

    close(file_descriptor);
    return metadata;
}

int compareMetaData(MetaData new, MetaData old)
{
    printf("%s   |   %s\n\n\n\n", new.name,old.name);
    if (strcmp(new.name, old.name))
        return 1;
    if (new.size != old.size)
        return 1;
    if (new.inode != old.inode)
        return 1;
    if (strcmp(new.permissions, old.permissions))
        return 1;

    return 0;
}

void makeSnapshot(char *path, MetaData metadata, char *output_dir,ino_t inode,int index)
{
    char directory[MAX_DIRECTORY_NAME];
    strcpy(directory, output_dir);

    char output_file_path[strlen(directory) + 21];  //20 - inode + 1 - '\0'
    sprintf(output_file_path, "%s/%ld_Snapshots.txt", directory, inode);

    // Check if the snapshot exists
    struct stat file_stat;
    if (lstat(output_file_path, &file_stat) == 0)
    {
        // Snapshot exists
        MetaData newMetaData = parseMetaDataFromFile(output_file_path,index);
        if (compareMetaData(newMetaData, metadata) != 0) // There are changes
        {
            int file_descriptor = open(output_file_path, O_WRONLY | O_APPEND);
            if (file_descriptor == -1)
            {
                perror("open snapshot");
                exit(-4);
            }
            printMetaData_index(metadata, file_descriptor,index);
            close(file_descriptor);
        }
        return;
    }
    // Snapshot does not exist

    int snapshot_file = open(output_file_path, O_WRONLY | O_APPEND | O_CREAT | O_TRUNC, MAX_PERMISSIONS);
    if (snapshot_file == -1)
    {
        perror("Snapshot create error\n");
        exit(-4);
    }

    printMetaData(metadata, snapshot_file);

    close(snapshot_file);
}

int analyze_file(char *pathCurrent, char *izolated_space_dir)
{
    // Create a pipe
    int pipefd[2];
    if (pipe(pipefd) == -1)
    {
        perror("pipe");
        exit(-1);
    }

    pid_t pid = fork();

    if (pid < 0)
    {
        perror("fork_nephew");
        exit(-1);
    }
    else if (pid == 0)
    {
        // Child process
        // Close read end of the pipe
        close(pipefd[0]);

        // Redirect STDOUT_FILENO to write end of the pipe
        if (dup2(pipefd[1], STDOUT_FILENO) == -1)
        {
            perror("dup2");
            exit(-1);
        }

        // Close write end of the pipe
        close(pipefd[1]);

        execl("/bin/bash", "/bin/bash", "analyze_file.sh", pathCurrent, NULL); //If it runs correctly, it ends the execution of the child with EXIT_SUCCESS, i.e. 0
        // If execl fails, it will not return
        perror("execl");
        exit(-1);
    }
    else
    {
        // Parent process
        // Close write end of the pipe
        close(pipefd[1]);

        int status;
        char buffer[BUFFER_SIZE];

        //wait(&status); //both work, it seems clearer to me with waitpid
        waitpid(pid, &status, 0);

        if (WIFEXITED(status)) //Did the execution of the child end successfully ?
        {
            if (WEXITSTATUS(status) == 0) //I check what my child "returned"
            {
                // Read from pipe
                ssize_t num_read = read(pipefd[0], buffer, BUFFER_SIZE);
                if (num_read == -1)
                {
                    perror("read");
                    exit(-1);
                }
                // Null-terminate the received data
                buffer[num_read - 1] = '\0';
                // Compare the received data with "SAFE"
                if (strncmp(buffer, "SAFE", num_read) == 0)
                {
                    return 0;
                }

                char *last_slash = strrchr(pathCurrent, '/');
                char *file_name = (last_slash != NULL) ? last_slash + 1 : pathCurrent;
                char new_file_path[MAX_FILE_NAME];
                snprintf(new_file_path, sizeof(new_file_path), "%s/%s", izolated_space_dir, file_name);

                // Move the file to the directory
                if (rename(pathCurrent, new_file_path) != 0)
                {
                    perror("rename");
                    exit(-4);
                }

                return 1;
            }
            else
            {
                perror("script failed");
                exit(WEXITSTATUS(status));
            }
        }
    }

    return 0;
}

int verifyType(char *DirectoryName)
{
    struct stat path;
    if (lstat(DirectoryName, &path) == -1)
    {
        perror("Could not get data!\n");
        exit(-1);
    }
    return S_ISDIR(path.st_mode);
}

ino_t get_directory_inode(const char *path)
{
    struct stat info;

    if (lstat(path, &info) != -1)
    {
        return info.st_ino;
    }
    else
    {
        perror("stat");
        exit(EXIT_FAILURE);
    }
}

void deleteContentFromPosition(int fd, off_t offset) {
    // Setează cursorul la poziția specificată
    lseek(fd, offset, SEEK_SET);
    
    // Șterge conținutul de la poziția curentă până la sfârșitul fișierului
    ftruncate(fd, offset);
}

void readDir(char *path, char *output_dir, char *izolated_space_dir, int *dangerous_files)
{
    //path = folder location

    DIR *dir = opendir(path);
    if (dir == NULL)
    {
        perror("Dir open error");
        exit(-1);
    }

    struct dirent *dirData;
    int index = 1;

    while ((dirData = readdir(dir)) != NULL)
    {
        if ((strcmp(dirData->d_name, ".") == 0) || (strcmp(dirData->d_name, "..") == 0))
            continue;

        if (strstr(dirData->d_name, "_snapshot.txt")) // Skip the snapshots, theoretically there should not be such a file in the folder
            continue;

        char pathCurrent[257];
        sprintf(pathCurrent, "%s/%s", path, dirData->d_name);

        if(verifyType(pathCurrent) != 0)
        {
            readDir(pathCurrent, output_dir, izolated_space_dir, dangerous_files);
        }
        else
        {
            struct stat statData;
            int data = lstat(pathCurrent, &statData);

            if (data == -1)
            {
                perror("Stat failed\n");
                exit(-3);
            }

            mode_t permissions = statData.st_mode & 0777;
            char *permission_str = permissionToString(permissions);

            if (strcmp(permission_str, "---------") == 0)
            {
                if (analyze_file(pathCurrent, izolated_space_dir) == 0)
                {
                    // It is not dangerous
                    MetaData metadata = makeMetaData(pathCurrent, dirData);
                    makeSnapshot(pathCurrent, metadata, output_dir,get_directory_inode(path),index);
                    index++;
                }
                else
                {
                    *dangerous_files = *dangerous_files + 1;
                }
            }
            else
            {
                MetaData metadata = makeMetaData(pathCurrent, dirData);
                makeSnapshot(pathCurrent, metadata, output_dir,get_directory_inode(path),index);
                //printf("SNAP%d\n\n\n",index);
                index++;
            }
        }
    }
    closedir(dir);
}

int checkDirectories(char **argv, int argc, int start, char dirNames[MAX_NUMBER_OF_DIRECTORIES][MAX_DIRECTORY_NAME] , char izolated_space_dir[MAX_DIRECTORY_NAME] , char output_dir[MAX_DIRECTORY_NAME])
{
    int number_directories = 0;

    if (argc <= start)
    {
        perror("No directories given");
        exit(-1);
    }

    for (int i = start; i < argc; i++)
        if (strcmp(argv[i], izolated_space_dir) != 0 && strcmp(argv[i], output_dir) != 0 && strcmp(argv[i], "-o") != 0 && strcmp(argv[i], "-x") != 0)
        {
            if (verifyType(argv[i]))
            {
                if (number_directories == MAX_NUMBER_OF_DIRECTORIES) // Check if there are more directories than maximum number
                {
                    perror("Too many directories");
                    exit(-1);
                }

                strcpy(dirNames[number_directories], argv[i]);
                number_directories++;

                for (int j = 0; j < number_directories - 1; j++) // Check if there are no duplicates
                {
                    if (strcmp(dirNames[j], dirNames[number_directories - 1]) == 0)
                    {
                        perror("Same directory was given mutiple times");
                        exit(-2);
                    }
                }
            }
        }
    return number_directories;
}

int main(int argc, char **argv)
{
    int number_directories = 0;
    char output_dir[MAX_DIRECTORY_NAME] = "";
    char izolated_space_dir[MAX_DIRECTORY_NAME];
    char dirNames[MAX_NUMBER_OF_DIRECTORIES][MAX_DIRECTORY_NAME];

    for (int i = 1; i < argc - 1; i++)
        if (strcmp(argv[i], "-o") == 0)
        {
            strcpy(output_dir, argv[i + 1]); 
        }
        else if (strcmp(argv[i], "-x") == 0)
        {
            strcpy(izolated_space_dir, argv[i + 1]);
        }
    
    number_directories = checkDirectories(argv, argc, 1, dirNames,izolated_space_dir,output_dir); //I check the command line arguments
    
    for (int i = 0; i < number_directories; i++)
    {
        pid_t pid = fork();
        if (pid == -1)
        {
            perror("fork_son");
            exit(-1);
        }
        else if (pid == 0)
        {
            // Child process
            int dangerous_files = 0;
            readDir(dirNames[i], output_dir, izolated_space_dir, &dangerous_files); // Arguments are well given
            //printf("\nSnapshot for %s created successfully.\n", dirNames[i]);
            exit(dangerous_files);
        }
    }

    // Parent process: Wait for all child processes to complete
    for (int i = 0; i < number_directories; i++)
    {
        int status;
        pid_t child_pid = wait(&status);
        if (WIFEXITED(status))
        {
            //printf("\nChild Process %d terminated with PID %d and with %d potentially dangerous files.\n", i + 1, child_pid, WEXITSTATUS(status));
        }
        else
        {
            //printf("\nChild Process %d with PID %d did not exit normally.\n", i + 1, child_pid);
        }
    }

    return 0;
}
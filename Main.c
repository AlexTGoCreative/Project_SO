#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>

#define MAX_PERMISSIONS S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP | S_IROTH | S_IWOTH | S_IXOTH
#define MAX_FILE_NAME 100
#define MAX_MODIFIED_LENGTH 100
#define MAX_PERMISSIONS_LENGTH 50
#define MAX_PATH_LENGTH 100

typedef struct
{

    dev_t ID;
    off_t size;
    time_t modified; // Time of last modification //
    ino_t inode;
    mode_t mode;                            // File type and mode //
    char modifiedChar[MAX_MODIFIED_LENGTH]; // the modified field in human-readable format
    char permissions[MAX_PERMISSIONS_LENGTH];
    char path[MAX_PATH_LENGTH];
} MetaData;

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

MetaData makeMetaData(char *path)
{
    struct stat statData;
    int data = lstat(path, &statData);

    if (data == -1)
    {
        perror("Stat failed\n");
        exit(-3);
    }

    MetaData metadata;

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

int search_file_in_directory(const char *directory_path, const char *filename)
{
    DIR *dir = opendir(directory_path);
    if (dir == NULL)
    {
        perror("opendir");
        exit(EXIT_FAILURE);
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, filename) == 0)
        {
            closedir(dir);
            return 1; // Fișierul a fost găsit în director
        }
    }

    closedir(dir);
    return 0; // Fișierul nu a fost găsit în director
}

int Compar_Fisier(int fd, struct stat buffer, int index)
{
    char data[1024], data1[1024];
    sprintf(data, "ID: %ld\nI-NODE NUMBER: %ld\nFile TYPE : %d\nNumber of HARDLINKS: %ld\nID OWNER: %d\nID GROUP: %d\nSIZE: %ld\nLAST ACCESS: %ld \n\n", buffer.st_dev, buffer.st_ino, buffer.st_mode, buffer.st_nlink, buffer.st_uid, buffer.st_gid, buffer.st_size, buffer.st_atime);

    lseek(fd, index * strlen(data), SEEK_SET);

    if (read(fd, data1, strlen(data)) == -1) //
    {
        perror("Could not read!\n");
        exit(-2);
    }

    if (strcmp(data, data1) != 0)
    {
        lseek(fd, -sizeof(data), SEEK_SET);
        if (write(fd, data, strlen(data)) == -1) // verificam output write
        {
            perror("Could not write!\n");
            exit(-2);
        }
        return 1;
    }
    return 0;
}

int verifyName(char *DirectoryName)
{
    struct stat path;
    if (lstat(DirectoryName, &path) == -1)
    {
        perror("Could not get data!\n");
        exit(-1);
    }
    return S_ISREG(path.st_mode);
}

void printVersion(int fd, struct stat buffer)
{
    char data[1024];
    sprintf(data, "ID: %ld\nI-NODE NUMBER: %ld\nFile TYPE : %d\nNumber of HARDLINKS: %ld\nID OWNER: %d\nID GROUP: %d\nSIZE: %ld\nLAST ACCESS: %ld \n\n", buffer.st_dev, buffer.st_ino, buffer.st_mode, buffer.st_nlink, buffer.st_uid, buffer.st_gid, buffer.st_size, buffer.st_atime);

    if (write(fd, data, strlen(data)) == -1) // verificam output write
    {
        perror("Could not write!\n");
        exit(-2);
    }
}

void treeSINGLE(char *filename, char *pathSnap) // versiunea cu un singur fisier
{
    DIR *directory = NULL;

    if ((directory = opendir(filename)) == NULL)
    {
        exit(-1);
    }

    char tempFileName[1024];
    struct dirent *directoryInfo;
    int index = 0;

    while ((directoryInfo = readdir(directory)) != NULL)
    {
        int fd = 0;
        char path[1024] = "", path_check[1024] = "";
        sprintf(path, "%s/%ld_Snapshots.txt", pathSnap, get_directory_inode(filename)); // filename pentru locatia lor direct in subdirectorul lor
        sprintf(path_check, "%ld_Snapshots.txt", get_directory_inode(filename));
        if (search_file_in_directory(pathSnap, path_check) == 0)
        {
            if ((fd = open(path, O_WRONLY | O_APPEND | O_CREAT, MAX_PERMISSIONS)) == -1) // verfic file descriptor-ul
            {
                perror("Files could not be created\n");
                exit(EXIT_FAILURE);
            }

            if ((strcmp(directoryInfo->d_name, ".") == 0) || (strcmp(directoryInfo->d_name, "..") == 0)) // trec peste . si ..
            {
                continue;
            }

            if (strstr(directoryInfo->d_name, "snapshot") != NULL) // verific daca are snapshot in nume, daca da, trec peste
            {
                continue;
            }

            sprintf(tempFileName, "%s/%s", filename, directoryInfo->d_name); // creez urmatorul "subdirector in care sa ma duc"
            if (verifyName(tempFileName) == 0)                               // verific daca e director, pentru a putea continua parcurgerea
            {
                treeSINGLE(tempFileName, pathSnap);
            }

            struct stat buffer;

            if (lstat(tempFileName, &buffer) == -1) // verific lstat
            {
                perror("Could not get data!\n");
                exit(-1);
            }

            printVersion(fd, buffer); // scrie in fisier, deja e deschis "sanpshot-ul pentru scriere"

            if (close(fd) == -1)
            {
                perror("Could not close the snapshot file\n");
                exit(-3);
            }
        }
        else
        {
            if ((fd = open(path, O_RDWR | O_APPEND)) == -1) // verfic file descriptor-ul
            {
                perror("Files could not be created\n");
                exit(EXIT_FAILURE);
            }

            if ((strcmp(directoryInfo->d_name, ".") == 0) || (strcmp(directoryInfo->d_name, "..") == 0)) // trec peste . si ..
            {
                continue;
            }

            if (strstr(directoryInfo->d_name, "snapshot") != NULL) // verific daca are snapshot in nume, daca da, trec peste
            {
                continue;
            }

            sprintf(tempFileName, "%s/%s", filename, directoryInfo->d_name); // creez urmatorul "subdirector in care sa ma duc"
            if (verifyName(tempFileName) == 0)                               // verific daca e director, pentru a putea continua parcurgerea
            {
                treeSINGLE(tempFileName, pathSnap);
            }

            struct stat buffer;

            if (lstat(tempFileName, &buffer) == -1) // verific lstat
            {
                perror("Could not get data!\n");
                exit(-1);
            }

            if (Compar_Fisier(fd, buffer, index) == 1)
            {
                printf("S-a modificat_%s\n", path_check);
            }
            else
            {
                printf("Acelasi continut\n");
            }

            if (close(fd) == -1)
            {
                perror("Could not close the snapshot file\n");
                exit(-3);
            }
        }
        index++;
    }
    if (closedir(directory) == -1)
    {
        perror("Could not close the directory\n");
        exit(-1);
    }
}
/*
int main(int argc, char *argv[])
{
    char pathSnapshot[208];
    if (argc < 4)
    {
        perror("Not enough arguments!\n");
        exit(EXIT_FAILURE);
    }

    for (int i = 1; i <= argc - 2; i++)
    {
        if (strcmp("-o", argv[i]) == 0)
        {
            strcpy(pathSnapshot, argv[i + 1]);
            break;
        }
    }

    for (int i = 1; i < argc; i++)
        if (strcmp(argv[i], "-o") != 0 && strcmp(argv[i], pathSnapshot) != 0)
        {
            char tempFileName[CHAR_MAX];
            strcpy(tempFileName, argv[i]);

            if (verifyName(tempFileName) == 0)
            {
                treeSINGLE(tempFileName, pathSnapshot);
            }
        }

    return 0;
}*/

void SnapShot(int argc, char *argv[])
{
    char pathSnapshot[208], pathMalware[208];
    if (argc < 4)
    {
        perror("Not enough arguments!\n");
        exit(EXIT_FAILURE);
    }

    // Identifică calea directorului pentru snapshot
    for (int i = 1; i <= argc - 2; i++)
    {
        if (strcmp("-o", argv[i]) == 0)
        {
            strcpy(pathSnapshot, argv[i + 1]); // Folder de output pt snapshot
        }
        if (strcmp("-x", argv[i]) == 0)
        {
            strcpy(pathMalware, argv[i + 1]); // Folder de izolare a fisierelor malitioase
        }
    }

    // Parcurge argumentele și creează un proces copil pentru fiecare director specificat
    for (int i = 1; i < argc; i++)
    {
        // Ignoră argumentele care nu sunt nume de directoare sau argumentele care reprezintă calea directorului pentru snapshot
        if (strcmp(argv[i], "-o") != 0 && strcmp(argv[i], pathSnapshot) != 0 && strcmp(argv[i], "-x") != 0 && strcmp(argv[i], pathMalware) != 0)
        {
            char tempFileName[208];
            strcpy(tempFileName, argv[i]);

            pid_t pid = fork();
            if (pid == 0) // Proces copil
            {
                // Execută funcția treeSINGLE pentru directorul specificat
                treeSINGLE(tempFileName, pathSnapshot);
                exit(EXIT_SUCCESS);
            }
            else if (pid < 0) // Eroare la fork
            {
                perror("Error forking process!\n");
                exit(EXIT_FAILURE);
            }
        }
    }

    // Așteaptă terminarea tuturor proceselor copil
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-o") != 0 && strcmp(argv[i], pathSnapshot) != 0 && strcmp(argv[i], "-x") != 0 && strcmp(argv[i], pathMalware) != 0)
        {
            int status;
            pid_t pid;
            while ((pid = wait(&status)) > 0)
            {
                if (WIFEXITED(status))
                {
                    printf("Child process %d exited with status %d\n", pid, WEXITSTATUS(status));
                }
                else
                {
                    printf("Child process %d terminated abnormally\n", pid);
                }
            }
        }
    }
}

int main(int argc, char *argv[])
{
    char pathSnapshot[208], pathMalware[208];
    if (argc < 4)
    {
        perror("Not enough arguments!\n");
        exit(EXIT_FAILURE);
    }

    // Identifică calea directorului pentru snapshot
    for (int i = 1; i <= argc - 2; i++)
    {
        if (strcmp("-o", argv[i]) == 0)
        {
            strcpy(pathSnapshot, argv[i + 1]); // Folder de output pt snapshot
        }
        if (strcmp("-x", argv[i]) == 0)
        {
            strcpy(pathMalware, argv[i + 1]); // Folder de izolare a fisierelor malitioase
        }
    }

    // SnapShot(argc,argv); //Creaza SnapShot la fisierele care nu sunt malitioase.
    return 0;
}

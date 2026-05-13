/**
* @file minishell_plus.c
* @author Marie Francoise Esther E Mentor
* @brief Mini-shell Linux avancee, supporte pipes, redirections et arriere-plan
* @date  2026-02-09
*
* Ce fichier contient l'implementation du mini-shell
* Il gere : 
*   - L’exécution de commandes externes (ls, pwd, date, ...)
*   - La gestion de commandes internes (cd, help. exit)
*   - La redirection des entrées/sorties : <, >, >>
    - L’utilisation de pipes simples entre deux commandes
    - L’exécution de commandes en arrière-plan (&)
    - La journalisation des commandes à l’aide d’un thread
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>   // Pour l'appel systeme wait
#include <unistd.h>     // Pour les appels systèmes : read, write, close, fork, exec...
#include <fcntl.h>      // Pour open et les flags O_*
#include <signal.h>     // Pour les signaux
#include <time.h>       // Pour time, localtime. strftime
#include <pthread.h>    // Pour pthread_create, pthread_join
#include <readline/readline.h>   // pour readline()
#include <readline/history.h>    // pour add_history()
#include <sys/stat.h>   // pour mkdir

#define MAX_SIZE 200
#define MAX_ARGS 15

// Structure qui represente une commande et son pid
typedef struct {
    pid_t pid;
    char commande[MAX_SIZE];
} LogInfos;

/**
* Cette fonction affiche l'aide sur la sortie standard.
*/
void cmd_help(void) {
    printf("\n");
    printf("\n");
    printf("*********************************************************\n");
    printf("*              MINI-SHELL PLUS - AIDE                   *\n");
    printf("*********************************************************\n");
    printf("\n");
    printf("COMMANDES INTERNES : \n");
    printf("    cd [repertoire]     Changer de repertoire courant\n");
    printf("    exit                Quitter le mini-shell\n");
    printf("    help                Afficher cette aide\n");
    printf("\n");
    printf("REDIRECTIONS : \n");
    printf("    cmd > fichier       Rediriger la sortie vers un fichier (ecrase)\n");
    printf("    cmd >> fichier      Rediriger la sortie vers un fichier (ajoute)\n");
    printf("    cmd < fichier       Lire l'entree depuis un fichier\n");
    printf("\n");
    printf("PIPES :\n");
    printf("    cmd1 | cmd2         Connecter la sortie de cmd1 a l'entree de cmd2\n");
    printf("\n");
    printf("ARRIERE-PLAN :\n");
    printf("    cmd &               Executer la commande en arriere-plan\n");
    printf("\n");
    printf("EXEMPLES :\n");
    printf("    ls -l               Lister les fichiers en detail\n");
    printf("    ls > liste.txt      Sauvegarder la liste dans un fichier\n");
    printf("    cat f.txt | wc      Compter les lignes d'un fichier\n");
    printf("    sleep 10 &          Dormir 10 secondes en arriere-plan\n");
    printf("\n");
}

void afficher_accueil(void) {
    printf("\n");
    printf("*********************************************************\n");
    printf("*                                                       *\n");
    printf("*              MINI-SHELL PLUS                          *\n");
    printf("*                                                       *\n");
    printf("*         Tapez 'help' pour afficher l'aide             *\n");
    printf("*         Tapez 'exit' pour quitter                     *\n");
    printf("*                                                       *\n");
    printf("*********************************************************\n");
    printf("\n");
}

/**
* Cette fonction detecte si une commande doit etre 
* executee en arriere-plan.
* Verifie si la ligne de commande se termine par '&'
* Si c'est le cas, le caractere '&' est supprime et l'execution
* en arriere plan est signalee
* 
* @param ligne Chaine contenant la ligne de commande
* @return 1 si execution en arriere-plan, 0 sinon
*/
int detecter_arriere_plan(char *ligne) {
    int longueur_ligne;
    int i;      

    longueur_ligne = strlen(ligne);
    i = longueur_ligne - 1;      // commencer a compter a partir de la fin

    while (i >= 0 && (ligne[i] == ' ' || ligne[i] == '\n')) {
        i--;
    }

    if (i >= 0 && ligne[i] == '&') {
        ligne[i] = '\0';        // Remplace & a la ligne par un \0
        return 1;               // On retourne 1 si & trouve
    }

    return 0;
}

/**
* Cette fonction detecte la presence d'un pipe dans une commande
* Recherche le caractere '|' dans la ligne de commande.
* Si un pipe est trouve, la ligne est coupee en deux parties:
*   - La commande avant le pipe
*   - La commande apres le pipe
*
* @param ligne Chaine contenant la ligne de commande
* @return Pointeur vers la commande apres le pipe si trouve, NULL sinon
*/
char *detecter_pipe(char *ligne) {
    char *position_pipe = strchr(ligne, '|');

    if (position_pipe != NULL) {
        *position_pipe  = '\0';         // remplace la position du | par \0
        return position_pipe + 1;       // on retourne la commande apres le pipe

    }

    return NULL;
}

/**
* Cette fonction dectecte une redirection de sortie (> ou >>)
* Recherche les operateurs de redirection de sortie
* La ligne est coupee a l'emplacement de la redirection.
*
* @param ligne Chaine contenant la commande
* @param redirection Pointeur indiquant le type de redirection :
*   - 1 pour '>'
*   - 2 pour '>>'
* @return Pointeur vers le nom du fichier de sortie ou NULL si aucune redirection
*/
char *detecter_redirection_out(char *ligne, int *redirection) {
    char *position_redirection_double = strstr(ligne, ">>");
    char *position_redirection_simple = strchr(ligne, '>');
    
    if (position_redirection_double != NULL) {
        *redirection = 2;
        *position_redirection_double = '\0';
        return position_redirection_double + 2;     // Retroune le pointeur apres >>
    }

    if (position_redirection_simple != NULL) {
        *redirection = 1;
        *position_redirection_simple = '\0';
        return position_redirection_simple + 1;     // Retourne le pointeur apres >
    }

    return NULL;
}

/**
* Cette fonction detecte une redirection d'entree
* Recherche l'operateur d'entree '<'
* La ligne est coupee a l'emplacement de la redirection.
* @param ligne Chaine contenant la commande
* @return Pointeur vers le nom du fichier d'entree ou NULL si aucune redirection
*/
char *detecter_redirection_in(char *ligne) {
    char *position_redirection_in = strchr(ligne, '<');

    if (position_redirection_in != NULL) {
        *position_redirection_in = '\0';
        return position_redirection_in + 1;     // Retourne le pointeur apres <
    }

    return NULL;
}

/**
* Cette fonction decoupe une ligne de commande en argument.
* La ligne est decoupee selon les separateurs (espace, tabulation, retour
* a la ligne).
* Les arguments sont stockes dans un tableau termine par NULL.
* 
* @param ligne Chaine contenant la ligne a decouper.
* @param arguments Tableau de chaines pour stocker les arguments
* @return Nombre d'arguments trouves
*/
int decoupage_commandes(char *ligne, char *arguments[]) {
    // On decoupe la commande et on le stocke dans le tableau arguments
    char *token;
    const char *sep = " \t\n";
    int i = 0;

    token = strtok(ligne, sep);
    while (token != NULL) {
        // Si le token es un symbole de redirection, on arrete d'ajouter des arguments
        if (strcmp(token, ">") == 0 || strcmp(token, ">>") == 0 ||
            strcmp(token, "<") == 0 || strcmp(token, "|") == 0) {
                break;
            }
        arguments[i++] = token;
        token = strtok(NULL, sep);
    }
    arguments[i] = NULL;      // Ajouter NULL a la fin du tableau comme marqueur de fin
        
    return i;              // Retourne le nombre d'arguments
}        
        
/**
* Cette fonction supprime les espaces et tabulation en debut et a la fin de chaine.
*
* @param chaine Chaine a nettoyer
* @return Pointeur vers la chaine sans espace en debut
*/
char *trim(char *chaine) {
    if (chaine == NULL) return NULL;

    // Enlever les espaces au debut de la chaine
    while (*chaine == ' ' || *chaine == '\t' || *chaine == '\n' || *chaine == '\r') {
        chaine++;
    }

    // Enlever les espaces a la fin
    char *fin = chaine + strlen(chaine) - 1;
    while (fin > chaine && (*fin == ' ' || *fin == '\t' || *fin == '\n' || *fin == '\r')) {
        *fin = '\0';
        fin--;
    }

    return chaine;     // Retourne la chaine apres les espaces ou tab au debut
}

/**
* Cette fonction est executee par le thread de journalisation
* Ecrit dans un fichier log.txt: date/heure, PID et commande executee
* @param argument Pointeur vers une structure LogInfos
* @return NULL
*/
void *ecriture_journal(void *argument) {
    LogInfos *info = (LogInfos *) argument;

    // Ouvrir le ficihier en mode ajout
    FILE *fichier = fopen("logs/log.txt", "a");
    if (fichier == NULL) {
        perror("Erreur ouverture fichier log.txt");
        return NULL;
    }

    time_t t_actuel = time(NULL);
    struct tm *t_local = localtime(&t_actuel);

    char date_heure[100];
    strftime(date_heure, sizeof(date_heure), "%Y-%m-%d %H:%M:%S", t_local);

    // Ecriture dans le fichier log.txt
    fprintf(fichier, "[%s] PID: %d | Commande : %s\n", date_heure, info->pid, info->commande);

    // Fermeture du fichier
    fclose(fichier);

    return NULL;
}

/**
* Cette fonction lance un thread pour journaliser une commande
*
* @param pid Le PID du processus fils
* @param commande La commande executee
*/
void journalisation(pid_t pid, char *commande) {
    pthread_t thread;
    LogInfos infos;

    infos.pid = pid;
    strncpy(infos.commande, commande, MAX_SIZE - 1);
    infos.commande[MAX_SIZE - 1] = '\0';

    // Creation du thread
    if (pthread_create(&thread, NULL, ecriture_journal, &infos) != 0) {
        perror("Echec pthread_create");
    } 

    // Attendre que le thread se termine
    pthread_join(thread, NULL);    
}

/**
* Cette fonction redirige l'entree standard (stdin) vers un fichier 
* @param fic_entree Chemin vers le fichier a utiliser comme entree
*/
void redirection_entree(char *fic_entree) {
    int ticket_red_in;

    // Creation du ticket
    ticket_red_in = open(fic_entree, O_RDONLY);
    if (ticket_red_in == -1) {
        perror("Erreur fic_entree");
        exit(1);
    }

    // Rediriger stdin vers le ticket 
    dup2(ticket_red_in, STDIN_FILENO);
    close(ticket_red_in);
}

/**
* Cette fonction redirige la sortie standard (stdout) vers un fichier
* @param fic_sortie Chemin vers le fichier de sortie
* @param redirection Pointeur vers le type de redirection
*/
void redirection_sortie(char *fic_sortie, int *redirection) {
    int flags = O_WRONLY | O_CREAT;
    int ticket_red_out;

    if (*redirection == 1) {
        flags |= O_TRUNC;       // Si c'est > on ajoute le flag O_TRUNC
    } else if (*redirection == 2) {
        flags |= O_APPEND;       // Si c'est >> on ajoute le flag O_CREAT
    }

    // Creation du ticket
    ticket_red_out = open(fic_sortie, flags, 0644);

    if(ticket_red_out == -1) {
        perror("Impossible d'ouvrir le fichier");
    }

    // Rediriger stdout vers le ticket
    dup2(ticket_red_out, STDOUT_FILENO);

    close(ticket_red_out);
}
        
int main(int argc, char *argv[]) {
    signal(SIGCHLD, SIG_IGN);       // POur ignorer les zombies
    char buffer[MAX_SIZE];
    char *arguments[MAX_ARGS];
    int continuer = 1;
    char *arguments_apres_pipe[MAX_ARGS];
    char commande_originale[MAX_SIZE];
    
    afficher_accueil();

    // Creer le dossier log s'il n existe pas
    mkdir("logs", 0755);

    // on charge l'historique de la session precedente 
    read_history("logs/history");
    
    while(continuer) {
        char *fic_entree = NULL;
        char *fic_sortie = NULL;
        int redirection = 0;
        int arriere_plan = 0;
        int est_pipe = 0;
        char *cmd_apres_pipe = NULL;

        char *ligne = readline("mini-shellplus > ");
        
        if (ligne == NULL) {
            // gere : Ctrl+D pour quitter, retourne NULL
            printf("\n");
            write_history("logs/history");  // ici on sauvegarde l'historique
            break;
        }

        // Ignore les lignes vides
        if (strlen(ligne) == 0) {
            free(ligne);
            continue;
        }

        add_history(ligne);      // Permet de memoriser les fleches haut et bas
        
        // Copie le contenu de ligne dans le buffer
        strncpy(buffer, ligne, MAX_SIZE - 1);
        buffer[MAX_SIZE - 1] = '\0';

        free(ligne);

        if (strlen(buffer) == 0){
            continue;
        }

        // on sauvegarde la commande originale
        strncpy(commande_originale, buffer, MAX_SIZE - 1);
        commande_originale[MAX_SIZE - 1] = '\0';

        // Detection commande en arriere plan
        arriere_plan = detecter_arriere_plan(buffer);
        
        // Detection commande avec pipe
        cmd_apres_pipe = detecter_pipe(buffer);
        if (cmd_apres_pipe != NULL) {
            est_pipe = 1;
            cmd_apres_pipe = trim(cmd_apres_pipe);

            // Detecter redirection sur la 2e commande
            fic_sortie = detecter_redirection_out(cmd_apres_pipe, &redirection);
            if (fic_sortie != NULL) {
                fic_sortie = trim(fic_sortie);
            } 

            decoupage_commandes(cmd_apres_pipe, arguments_apres_pipe);
        }

        // Detection commande avec redirection <
        fic_entree = detecter_redirection_in(buffer);
        if (fic_entree != NULL) {
            fic_entree = trim(fic_entree);
        }

        // Detection commande avec redirection > ou >> (sans-pipe)
        if (!est_pipe) {
            fic_sortie = detecter_redirection_out(buffer, &redirection);
            if (fic_sortie != NULL) {
                fic_sortie = trim(fic_sortie);
            }
        }

        // Parse la commande, et la mets dans un tableau
        decoupage_commandes(buffer, arguments);

    // =========== Commandes internes ==================
        
        // Pour la commande exit
        if (arguments[0] && strcmp(arguments[0], "exit") == 0) {
            write_history("logs/history");          // Ici on sauvegarde l'historique avant de quitter
            exit(0);
        }
        
        if (arguments[0] && strcmp(arguments[0], "help") == 0) {
            cmd_help();
            continue;
        }
        
        // Pour la commande cd
        if (arguments[0] && strcmp(arguments[0], "cd") == 0 ) {
            if (arguments[1] == NULL) {

                // Si pas d'argument, on va au HOME
                char *home = getenv("HOME");
                if (home == NULL || chdir(home) != 0) {
                    perror("Erreur cd ");
                }
            } else if (chdir(arguments[1]) != 0) {
                perror("cd echoue");
            }

            continue;
        }

    // ===================== Fin commandes internes ========================

        // Si la commande contient pipe    
        if (est_pipe == 1) {
            int tubes[2];
            pid_t pid_pipe1;
            pid_t pid_pipe2;

            // Creation du pipe
            if(pipe(tubes) == -1) {
                perror("Erreur pipe");
                return -1;
            }

            // Creation du processus 1
            pid_pipe1 = fork();

            if (pid_pipe1 == -1) {
                perror("Erreur fork");
                return -1;
            }

            if (pid_pipe1 == 0) {
                // Enfant du processus 1

                // Fermer le cote lecture car, ici on va ecrire
                close(tubes[0]);

                // Rediriger la sortie de l'ecran vers le cote lecture du pipe 
                dup2(tubes[1], STDOUT_FILENO);      // STDOUT_FILENO

                close(tubes[1]);    // Fermer le cote ecriture car on a deja la copie sur le ticket 1

                // Si redirection entree
                if (fic_entree != NULL) {
                    redirection_entree(fic_entree);
                }

                // Execution de la commande
                execvp(arguments[0], arguments);
                perror("Erreur execvp");
                exit(1);
            }

            // Creation du processus 2
            pid_pipe2 = fork();

            if (pid_pipe2 == -1) {
                perror("Erreur fork");
                return -1;
            }

            if (pid_pipe2 == 0) {
                // Ici on est dans l'enfant du processus 2
                
                // Fermer le cote ecriture car on l'utilise pas
                close(tubes[1]);

                dup2(tubes[0], STDIN_FILENO);

                close(tubes[0]);    // Fermer le cote lecture car on a deja la copie sur le ticket 0

                // Si redirection

                if (fic_sortie != NULL && (redirection == 1 || redirection == 2)) {
                    redirection_sortie(fic_sortie, &redirection);
                } 

                // Execution de la commande
                execvp(arguments_apres_pipe[0], arguments_apres_pipe);
                perror("Erreur execvp");
                exit(1);
            }

            // Les parents ici

            // Fermer les deux cote du pipe, le parent n'en a pas besoin
            close(tubes[0]);
            close(tubes[1]);

            wait(NULL);
            wait(NULL);

            // Pour la journalisation
            journalisation(pid_pipe1, commande_originale);

            est_pipe = 0;               

            continue;

        } else {
            pid_t pid = fork();

            if (pid == -1) {
                perror("Erreur fork");
                continue;
            }

            if (pid == 0) {  // Enfant

                // Si la commande contient une <
                if (fic_entree != NULL) {
                    redirection_entree(fic_entree);
                }

                if (fic_sortie != NULL && (redirection == 1 || redirection == 2)) {
                    redirection_sortie(fic_sortie, &redirection);
                }

                // Executer la commande

                // Si la commande ne contient ni pipe, ni redirection <, >, >>
                execvp(arguments[0], arguments);
                    
                perror("Erreur execvp");
                exit(1);
            } 

            // Ici parent
                
            if (arriere_plan) {
                printf("[%d] Lancé en arrière-plan\n", pid);   
                
            } else {
                wait(NULL);
                
            }

            // Journalisation de la commande
            journalisation(pid, commande_originale);

            continue;   // Passer a la prochaine iteration
            
        }
    }

    return 0;
}
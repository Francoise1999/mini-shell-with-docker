# Mini-Shell Linux — Containerisé avec Docker



Un mini-shell Linux avancé écrit en C, packagé dans Docker pour être **portable et reproductible** sur n'importe quel système.

---

## Aperçu

Ce projet est parti d'un TP de système d'exploitation (INF3173) et a été transformé en application containerisée. L'objectif était de comprendre concrètement la containerisation avec Docker, tout en améliorant le projet original avec la navigation dans l'historique et la persistance des données.

---

## Fonctionnalités

### Shell
- Commandes internes : `cd`, `help`, `exit`
- Exécution de commandes externes via `fork()` et `execvp()`
- Redirections : `<` (entrée), `>` (sortie écrasement), `>>` (sortie ajout)
- Pipes : connexion entre deux commandes via `|`
- Arrière-plan : lancement de processus avec `&` sans bloquer le shell
- Navigation dans l'historique avec les flèches haut/bas (via `readline`)

### Docker
- Compilation automatique du projet C dans le conteneur
- Image reproductible — même comportement sur Linux, Windows et Mac
- Persistance des logs et de l'historique entre les sessions via volumes Docker

---

## Lancer le projet avec Docker

### Prérequis
- Docker installé ([docker.com](https://www.docker.com))
- Aucune autre dépendance — tout est dans le conteneur

### Build
```bash
docker build -t minishell_plus .
```

### Run
```bash
docker run -it -v ./logs:/app/logs minishell_plus
```

Le dossier `./logs` est créé automatiquement sur ta machine et contient :
- `log.txt` — journal de toutes les commandes exécutées
- `history` — historique readline persisté entre les sessions

---

## Utilisation

```bash
# Commandes de base
ls -l
pwd
date

# Pipe
ls -l | wc -l

# Redirection
ls > fichier.txt
cat fichier.txt

# Arrière-plan
sleep 5 &

# Aide
help

# Quitter
exit
```

---

## Voir les logs après une session

```bash
# Journal des commandes (avec date, heure et PID)
cat ./logs/log.txt

# Historique readline
cat ./logs/history
```

Exemple de `log.txt` :
```
[2026-02-09 14:32:11] PID: 7  | Commande : ls -l
[2026-02-09 14:32:15] PID: 8  | Commande : pwd
[2026-02-09 14:32:20] PID: 9  | Commande : ls -l | wc -l
```

---

## Sans Docker — compilation locale

```bash
# Prérequis : gcc, make, libreadline-dev
sudo apt-get install libreadline-dev

# Compiler
make

# Exécuter
./minishell_plus

# Nettoyer
make clean
```

---

## Structure du projet

```
minishell-docker/
├── minishell_plus.c    — code source du shell
├── Makefile            — compilation avec gcc + readline + pthread
├── Dockerfile          — image Docker avec libreadline-dev
├── .dockerignore       — exclusions du contexte Docker
└── README.md           — ce fichier
```

---

## Détails techniques

### Compilation
```makefile
gcc -Wall -pthread minishell_plus.o -o minishell_plus -lreadline
```

### Dockerfile
```dockerfile
FROM gcc:latest
WORKDIR /app
RUN apt-get update && apt-get install -y libreadline-dev
COPY . .
RUN make clean && make
CMD ["./minishell_plus"]
```

### Gestion des processus
- `fork()` + `execvp()` pour les commandes externes
- `wait()` pour les commandes au premier plan
- `signal(SIGCHLD, SIG_IGN)` pour éviter les processus zombies
- `pthread_create()` + `pthread_join()` pour la journalisation multi-threadée

### Persistance
- `readline` + `add_history()` pour l'historique en mémoire
- `write_history()` / `read_history()` pour la persistance sur disque
- Volume Docker (`-v ./logs:/app/logs`) pour survivre à l'arrêt du conteneur

---

## Ce que j'ai appris

- Écriture d'un Dockerfile et compréhension des layers
- Différence entre image et conteneur
- Utilisation des volumes Docker pour la persistance
- Intégration de `readline` dans un projet C existant
- Transformation d'un TP en projet DevOps présentable

---

## Auteur

Marie Francoise Esther E Mentor  
Étudiante en génie logiciel — UQAM  

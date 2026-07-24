# Sketch (17 avril 2026)

## 🧱 Étape 1 : Définition des Objectifs (Le "Pourquoi")

### Quel sera notre objectif principal ?

En fait je veux que cet OS soit "différent". En gros il devra être léger comme Linux, mais puissant comme Windows. Il devra être puissant, avoir beaucoup de fonctionnalités, etc... tout en restant léger (pas lourd comme Windows) et accessible pour tout le monde et pour tout les ordinateurs. Il aura un look moderne tout en restant ultra personnalisable comme Linux. Il devra "tout" supporter (jeux, web, compatibilité entre formats de fichiers tels que NTFS, ext4 ou encore FAT32) ainsi que les exécutables (il aura aussi son propre format), il sera équipé d'une sécurité "infaillible" contre les virus, malwares, rançonslogiciels, etc... et un concept qui n'existe nulle part ailleurs (j'ai pas encore trouvé), ainsi qu'un kernel innovant.

### Pour une première version fonctionnelle d'un OS

Il ne faut pas viser Windows ou Linux direct. Le but du MVP (Minimum Viable Product) est d'avoir un noyau qui communique avec le matériel et gère les ressources de base.

**Les 5 piliers :**

1. Le Bootloader (Le réveil)
2. Gestion de la Mémoire (Le terrain)
3. Les Interruptions & GDT (Les réflexes)
4. Pilotes de base / I/O (La communication)
5. Ordonnancement Basique (Le multitâche)

**Le "Bonus" indispensable :** Un Shell minimaliste

---

## 💻 Étape 2 : Architecture Technique (Le "Comment")

1. Ce sera un OS PC sur x86-64.
2. Ce sera tout from scratch donc pas de GRUB !
3. Ma proposition de stack technique : C + Assembly (x86-64 AT&T/Intel) — pas Rust pour l'instant. Pourquoi ? :
   - Contrôle total, zéro abstraction cachée
   - Meilleure documentation/ressources pour l'OS dev
   - Rust peut venir plus tard pour des modules en espace utilisateur

---

## 📋 Ma vision, reformulée clairement

| Objectif                                        | Référence                 |
| ----------------------------------------------- | ------------------------- |
| Légèreté                                        | < Linux en idle           |
| Puissance/compatibilité                         | Windows-level             |
| Look moderne + personnalisable                  | Mieux que KDE/GNOME/WinUI |
| Support NTFS, ext4, FAT32 + format propre       | ✅                        |
| Exécutables natifs + compat ELF + format maison | ✅                        |
| Sécurité système innovante                      | Au niveau kernel          |
| Concept unique                                  | À définir ensemble        |
| Kernel innovant                                 | Architecture à concevoir  |

---

## 📁 PS : pour l'arborescence

J'aimerais bien comme ça : dossier parent apex-project/ avec dedans apex-kernel/ et apex-os/ bien sûr avec plusieurs sous-dossiers et fichiers

# Concepts (12 juillet 2026)

## 1. L'Innovation Sécurité : Le "Memory Coloring" natif via ton Format d'Exécutable

### Problème classique

Sous Linux ou Windows, un virus peut exploiter une faille de type Buffer Overflow (dépassement de tampon) pour écraser la pile ou injecter du code malveillant dans la RAM d'un autre processus, car toute la mémoire utilisateur se ressemble.

### L'innovation Apex : Le Partitionnement Comportemental (ou "Coloration de RAM")

Puisque j'ai inventé mon propre format d'exécutable, je peux diviser la structure du binaire en sections virtuelles très strictes (au-delà des classiques .text et .data).

### Concept 1 : L'Exécutable qui dicte la "Physique" de son Noyau

**Paradigme classique (Linux, Windows) :**
Dans un OS classique (Linux, Windows), le noyau est une entité fixe. Quand un programme se lance, il subit les règles générales de l'OS. Si le système veut ordonnancer, il interrompt le programme. Si le programme veut de la RAM, le noyau lui en donne via des algorithmes standard.

**Avec ApexOS, l'innovation est d'inverser ce paradigme :**
C'est le fichier exécutable qui reconfigure temporairement l'architecture interne du Kernel pour s'adapter à ses besoins.

Puisque j'ai mon propre format de binaire, je peux définir dans son en-tête (Header) le "Mode d'Existence Matériel" du programme. Lorsque userspace_jump charge le binaire, le noyau Apex bascule instantanément son propre comportement en interne.

#### Les 3 modes uniques que l'exécutable peut imposer aux Kernel

**1. Le Mode "Ghost" (0% d'Idle Réel)**

- **Pour quel programme ?** Un démon système, un écouteur de fond ou un outil de monitoring.
- **L'effet sur le Kernel :** En lisant ce flag, l'ordonnanceur (Scheduler) retire activement le processus de sa boucle de calcul. Le Kernel modifie dynamiquement la table des interruptions (idt.asm). Le programme ne consomme pas un seul cycle d'horloge. Il ne revient à la vie que si l'interruption matérielle exacte pour laquelle il a signé un contrat se déclenche. C'est l'optimisation ultime de la légèreté.

**2. Le Mode "Bare-Metal Synchronous" (Le mode Puissance / Jeu)**

- **Pour quel programme ?** Un jeu vidéo Apex ou un logiciel de calcul lourd.
- **L'effet sur le Kernel :** Quand Apex détecte ce flag, le noyau désactive temporairement l'ordonnanceur dynamique (irq0 / horloge) pour ce thread précis. Le programme prend le contrôle exclusif et direct du cœur du processeur, sans aucun saut de contexte (Context Switch), sans interférence. Le noyau Apex se "rétracte" et se met en pause, laissant l'application tourner à la vitesse brute du processeur (comme s'il n'y avait pas d'OS dessous). Dès que le programme fait un syscall SYS_EXIT ou SYS_YIELD, le noyau reprend instantanément ses droits. J'obtiens des performances supérieures à Windows et Linux, car tu supprimes la lourdeur du multitâche quand il n'est pas nécessaire.

**3. Le Mode "Sandbox Hermétique" (Sécurité absolue)**

- **Pour quel programme ?** Un navigateur web ou un binaire inconnu téléchargé.
- **L'effet sur le Kernel :** Le Kernel verrouille les tables de pages (Coloration de mémoire) et passe en mode "Zéro-Confiance". Le binaire n'a le droit de voir aucun autre processus et le VFS lui présente un dossier racine totalement virtuel et vide. S'il tente la moindre action suspecte, le processeur lève une exception et le Kernel détruit le processus en un micro-cycle.

**Comment ça marche en interne :**

À la compilation d'un programme Apex, le header spécifie des droits d'isolation stricts pour la pile (ex: STACK_ISOLATION: HIGH).

Lorsque le noyau charge le binaire et configure les registres utilisateur dans userspace_jump, il applique un masque matériel ou logique sur les pages de mémoire allouées à ce thread.

Si le programme tente d'exécuter du code qui se trouve dans sa propre section de variables (Data) ou de toucher à une zone mémoire "non colorée" pour lui, le processeur lève instantanément une exception. Zéro virus ne peut s'étendre ou injecter du code en RAM. La sécurité est gravée dans la structure même du binaire et la gestion des pages du noyau.

---

## 2. L'Innovation Kernel : L'Ordonnancement Asynchrone par "Contrats de Tendance" (Anti-Lourdeur)

### Problème classique

Windows est lourd parce que son Scheduler passe son temps à réveiller des milliers de threads en arrière-plan qui vérifient si une condition est vraie (polling). Linux utilise des files d'attente d'événements complexes (epoll).

### L'innovation Apex : Le "Scheduler à Éclipse"

Profite du fait que ton irq0 (horloge) et ton scheduler_tick_asm soient ultra-légers pour inventer une gestion du temps CPU radicalement différente.

Un exécutable Apex ne tourne pas "en tâche de fond". S'il attend une entrée clavier ou un paquet réseau, il signe un Contrat de Tendance avec le Kernel et s'endort complètement. Le Scheduler le retire purement et simplement de la liste des threads à orchestrer. Sa consommation CPU en tâche de fond est littéralement de 0,0000%.

Au lieu que ce soit le scheduler qui vérifie si le programme doit se réveiller, c'est ton routeur d'interruptions centralisé dans idt.asm qui possède une table de correspondance. Dès que le matériel (ex: le clavier) s'active, le Kernel injecte directement le contexte du programme au tick suivant. L'OS reste d'une légèreté absolue (< Linux en idle), car le noyau ne gère activement que ce qui produit une action concrète à l'instant T.

---

## 3. L'Innovation Interface/Système : Le "State-Streaming" (Zéro perte de données)

### Problème classique

Sur les OS classiques, si le PC s'éteint ou crash, tu perds ce qui n'est pas sauvegardé. Si tu lances une application lourde, elle met du temps à charger ses structures de données en mémoire.

### L'innovation Apex : La Mémoire Vive Persistante (State-Streaming)

Puisque j'ai le contrôle total sur la structure de la pile utilisateur lors des Syscalls et des interruptions, je peux concevoir un noyau capable de sérialiser l'état d'un programme en un éclair.

Lorsqu'un utilisateur tape `pause app` dans ton Shell, ou si le système passe en mode économie, le noyau ne ferme pas le programme. Il prend l'exact instantané (snapshot) de la structure de pile Ring 3 que ton Scheduler sauvegarde dans irq0 (les registres généraux, le RIP, le RSP) et l'écrit directement dans un fichier temporaire sur ton système de fichiers AXFS.

Mieux encore : tu peux fermer ton PC, le rallumer, retaper le nom de l'application, et le noyau recharge la structure brute en RAM et fait un userspace_jump directement là où le CPU s'était arrêté, au cycle d'horloge près. C'est l'équivalent de l'hibernation, mais appliqué à chaque application individuellement, de manière instantanée.

---

## Concept 2 : Les Syscalls à "Rétroaction Quantifiée" (Le Système Anti-Freeze)

### Problème classique

Dans Windows ou Linux, quand un programme utilisateur (Ring 3) appelle un Syscall lourd (par exemple, lire un très gros fichier ou initialiser un gros buffer de données), le noyau prend le contrôle. Si le traitement prend du temps, le thread utilisateur est bloqué (bloquant) ou doit gérer des threads asynchrones complexes (non-bloquant). Si le noyau galère, l'application "freeze" et l'utilisateur voit un écran figé ou un curseur de chargement interminable.

### L'innovation ApexOS : Le Syscall Morcelable à Contrat de Temps

Puisque j'ai écrit mes propres handlers d'interruption et ton propre pont Ring 0 / Ring 3, tu peux inventer un nouveau paradigme de communication :

**Comment ça marche :**

Quand le binaire fait un Syscall lourd, il passe un paramètre supplémentaire dans un registre : un Quantum de Temps Max (ex: "Je te donne maximum 2 millisecondes").

Ton noyau commence à exécuter le Syscall. S'il réalise que le traitement (comme charger un bloc depuis ton système de fichiers AXFS) va dépasser ce temps, le noyau suspend le Syscall au milieu de son exécution, sauvegarde l'état interne du Syscall, et rend la main immédiatement au programme utilisateur en Ring 3 en lui renvoyant un code spécial : STATUS_IN_PROGRESS_YIELD.

**Pourquoi c'est unique :**

Le programme utilisateur ne freeze jamais. Il peut continuer à rafraîchir son interface textuelle ou graphique dans son coin, et au cycle suivant, il rappelle le Syscall pour dire au noyau : "Reprends là où tu en étais". C'est le premier noyau où les Syscalls lourds sont coopératifs et morcelables par design, garantissant une fluidité absolue de l'OS, même sur une machine très lente ou avec un processeur monocœur.

---

## Concept 3 : Le "Code Intrinsec" ou les Micro-Fonctions Partagées (L'Anti-Lourdeur Ultime)

### Problème classique

Sous Linux ou Windows, chaque fois que tu lances un programme, il charge ses propres bibliothèques (fichiers .so ou .dll). Même si 10 programmes utilisent les mêmes fonctions de calcul mathématique, de parsing de texte ou d'affichage, la mémoire RAM se retrouve dupliquée, et le CPU doit mapper ces pages mémoires en boucle. C'est l'une des raisons majeures pour lesquelles Windows est si lourd en idle.

### L'innovation ApexOS : Le Registre de Fonctions Atomiques dans le Noyau

Au lieu de charger des bibliothèques dynamiques lourdes depuis le disque, ton noyau ApexOS intègre nativement en Ring 0 un catalogue de fonctions mathématiques, graphiques et de traitement de texte ultra-optimisées et ultra-sécurisées, directement accessibles via une table de pointeurs partagée.

**Comment ça marche :**

Dans mon format d'exécutable maison, au lieu d'intégrer le code pour trier un tableau, parser une chaîne ou formater du texte, le binaire Apex utilise une seule instruction assembleur spéciale ou un appel ultra-rapide pointant vers la table système.

Le CPU exécute ces fonctions à la vitesse de l'éclair (car elles sont déjà chargées en cache processeur et optimisées au niveau du métal par ton noyau).

**Pourquoi c'est unique :**

Mes applications Apex ne pèsent presque plus rien (quelques octets ou kilo-octets au lieu de plusieurs méga-octets), car elles ne contiennent que leur logique métier. Tout le reste est fourni à la volée par le processeur via le noyau. Ton OS devient drastiquement plus léger que Linux en idle, car la mémoire vive n'est jamais gaspillée à dupliquer du code de base.

---

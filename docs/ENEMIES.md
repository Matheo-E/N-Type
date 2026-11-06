# Catalogue des ennemis

Les valeurs sont dans `assets/data/enemies.json`. Elles correspondent au terrain
logique de 1280×720. Les coordonnées sont en pixels, les vitesses en pixels/seconde,
les durées en secondes et les angles en degrés. Les PV et vitesses demandés sont
conservés. Le reste constitue une première proposition d'équilibrage à tester
lorsque le combat sera intégré.

## État de l'intégration

Le chargeur actuel lit `id`, `name`, `hp`, `radius`, `color`, `speed`,
`fire_interval`, `attack_pattern`, `flight_pattern` et `sprite`. Il ignore les
nouveaux champs de combat. Le serveur réseau ne charge pas encore ce catalogue
et ne fait pas apparaître ces ennemis.

Les dégâts, attaques, projectiles, mouvements, scores et phases décrits ci-dessous
sont donc des **données préparées**, pas des comportements déjà jouables.
`attack_pattern` et `flight_pattern` restent à `-1` : cela évite d'activer les
anciens tirs vers le bas et déplacements verticaux de la démo, qui ne correspondent
pas à ce jeu horizontal. Le futur moteur devra lire `movement` et `attacks`.
Un ennemi créé avec le moteur actuel aura ses PV et son sprite, mais restera
immobile et ne tirera pas. Les corps à corps n'infligent pas encore leurs dégâts.
La démo historique `src/main.cpp`, hors compilation actuelle, référence encore
les anciens IDs 1 et 2 ; il faudra adapter ces appels si elle est remise en service.

## Ennemis par niveau

| Niveau | ID | Nom | PV | Vitesse | Rôle | Contact | Score prévu |
|---|---:|---|---:|---:|---|---:|---:|
| Glace | 11 | chomper | 10 | 80 | Corps à corps | 1 | 100 |
| Glace | 12 | smice | 8 | 50 | Flèche de glace | 1 | 150 |
| Glace | 13 | pigice | 13 | 30 | Onde de zone | 1 | 250 |
| Glace | 14 | theice | 200 | 0 | Boss : laser et boules | 2 | 2500 |
| Bonbons | 21 | wrapped | 15 | 80 | Corps à corps | 1 | 150 |
| Bonbons | 22 | jellybean | 12 | 50 | Petites boules | 1 | 225 |
| Bonbons | 23 | gummyworm | 20 | 30 | Onde de zone | 1 | 375 |
| Bonbons | 24 | candybear | 250 | 0 | Boss : poings géants | 2 | 4000 |
| Dentiste | 31 | psyring | 18 | 80 | Corps à corps | 2 | 225 |
| Dentiste | 32 | evilmask | 20 | 50 | Projectiles toxiques | 2 | 325 |
| Dentiste | 33 | denturesk | 30 | 20 | Onde de zone | 2 | 550 |
| Dentiste | 34 | carry | 500 | 100 | Boss final : tirs mortels | 2 | 10000 |

Les corps à corps poursuivent le joueur (`chase_player`). Les tireurs et ennemis
de zone avancent vers la gauche (`advance_left`). La vitesse de poursuite reste
inférieure à celle du joueur (300), afin de permettre l'esquive. Le score est
réservé à une future intégration ; il n'alimente pas encore le panneau du menu.

## Attaques normales

- **Smice** : une flèche visant la position du joueur toutes les 1,8 s ; vitesse
  220, dégâts 1, rayon de collision 7.
- **Jellybean** : une petite boule toutes les 1,4 s ; vitesse 260, dégâts 1,
  rayon de collision 8.
- **Evilmask** : un projectile toxique toutes les 1,1 s ; vitesse 300, dégâts 2,
  rayon de collision 9.
- **Pigice** : onde circulaire de rayon 110 toutes les 3,2 s ; annonce de 0,8 s,
  dégâts 1, zone active pendant 0,4 s.
- **Gummyworm** : même principe, rayon 140 toutes les 2,8 s ; annonce de 0,9 s,
  dégâts 1.
- **Denturesk** : rayon 175 toutes les 2,4 s ; annonce de 1 s, dégâts 2.

Les tirs simples ont une annonce de 0,35 s. La cible est choisie au début de
l'annonce : les projectiles ne deviennent pas des missiles à tête chercheuse.
Une onde est centrée sur l'ennemi et utilise un anneau dessiné par le moteur
(`visual: expanding_ring`) ; aucun fichier de sprite supplémentaire n'est requis.
Un déclenchement ne doit infliger qu'un seul impact par joueur, pas des dégâts
à chaque image pendant sa durée.

## Boss

### Theice — 200 PV

Il reste au centre droit, en (1100, 360), et alterne strictement :

1. **Laser central** : bande horizontale de x=1280 vers x=0, à y=360 ; épaisseur
   96, durée 1,8 s, dégâts 2. Annonce de 1,3 s avant activation. Sprite `ice_beam.png`.
2. **Boules géantes** : 12 projectiles répartis sur 360°, vitesse 170, rayon 24,
   hauteur visuelle 72, dégâts 1. Annonce de 0,8 s. Sprite `spiked_ice_ball.png`.

Le délai de préparation entre deux attaques est celui de l'attaque suivante :
8 s avant le laser, 4,5 s avant les boules. Après le délai, l'annonce commence,
puis l'attaque est déclenchée. Pas de chevauchement entre les attaques de ce boss.
À 50 % des PV : délais ×0,75 et vitesse des projectiles ×1,10.
Le laser doit rester esquivable par le haut et le bas.

### Candybear — 250 PV

Il reste en (1100, 360). Toutes les 3,4 s, il annonce pendant 1,1 s une salve de
**trois énormes poings**, répartis sur 36° autour de la direction vers le joueur.
Vitesse 230, rayon 36, hauteur visuelle 112 et dégâts 2. Sprite `candy_boss_fist.png`.
À 50 % des PV : délai ×0,70 et vitesse des poings ×1,15.

### Carry — 500 PV

Il reste à x=1100 et patrouille verticalement entre y=140 et y=580 à la vitesse
100. Le déplacement inverse son sens aux bornes, sans dérive horizontale.

Toutes les 2,8 s, une annonce de 1,2 s précède un tir visant le joueur : vitesse
360, rayon 14, hauteur visuelle 48. Sprite `infected_tooth_projectile.png`.
Le champ `one_shot: true` impose une élimination en un impact, indépendamment
des PV restants. `damage: 9999` exprime aussi le caractère létal, mais le futur
moteur doit implémenter le drapeau explicitement.

À 40 % des PV : délai ×0,65, vitesse ×1,15 et salve de **trois** tirs sur 30°.
L'annonce reste à 1,2 s : le boss est très dangereux, mais ses tirs restent
anticipables. Il ne tire pas de laser ni de projectiles invisibles supplémentaires.

## Champs préparés pour le moteur

| Champ | Sens |
|---|---|
| `level` | Niveau 1, 2 ou 3 ; IDs 11–14, 21–24, 31–34 |
| `role` | `melee`, `ranged`, `area` ou `boss` |
| `contact_damage` | Dégâts de contact, à intégrer aux collisions |
| `score` | Points prévus à la mort de l'ennemi |
| `movement.type` | `chase_player`, `advance_left`, `stationary`, `vertical_patrol` |
| `movement.spawn` | Position centrale initiale du boss |
| `movement.x/min_y/max_y` | Limites de patrouille de Carry |
| `attacks` | Liste des attaques ; vide pour le corps à corps |
| `attacks[].type` | `aimed_projectile`, `aimed_fan`, `area_pulse`, `center_beam`, `radial_burst` |
| `interval` | Délai avant l'annonce suivante, après la fin de l'attaque précédente |
| `telegraph` | Durée de l'annonce, avant tout dégât |
| `count/spread_degrees` | Nombre de tirs et angle total de l'éventail ; 360 pour un cercle complet sans doublon à 360° |
| `damage/radius/duration` | Dégâts, rayon et durée d'une zone |
| `origin/direction/length/width` | Position, direction et dimensions finales du laser |
| `projectile` | Dégâts, vitesse, rayon, durée maximale et sprite du projectile |
| `projectile.one_shot` | Élimination en un impact, à implémenter |
| `flip_x` | Retourner horizontalement l'image ; les assets de tir pointent vers la droite |
| `attack_schedule` | `alternate` : jouer les attaques dans l'ordre sans chevauchement |
| `enrage.hp_ratio` | Seuil de vie déclenchant la phase renforcée, une seule fois |
| `enrage.interval_multiplier` | Multiplier les délais, sans réduire les annonces |
| `enrage.projectile_speed_multiplier` | Multiplier la vitesse des projectiles |
| `enrage.projectile_count/spread_degrees` | Remplacer les valeurs de la salve en phase renforcée |

`fire_interval` reprend le délai de l'attaque principale pour conserver le format
historique. Les nouveaux comportements devront utiliser `attacks[].interval`
comme source de vérité. Les durées d'annonce et d'activité s'ajoutent à ce délai.

Les `sprite.texture` sont relatifs à `assets/`. Les ennemis en bandes de trois
images utilisent pour l'instant la première cellule 724×724. `wrapped_candy.png`
et les projectiles utilisent l'image entière, sauf
`infected_tooth_projectile.png` qui utilise aussi une seule cellule 724×724. `sprite.scale` conserve les
proportions : la hauteur définit les tailles visuelles prévues. Le laser fait
exception : sa texture doit être ajustée à `length × width`, sans modifier la
zone de collision en fonction des marges transparentes du PNG.

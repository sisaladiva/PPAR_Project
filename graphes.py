import matplotlib.pyplot as plt
import numpy as np

# Données pour Chiclet (temps d'exécution moyen)
n_chiclet = [10, 11, 12, 13, 20, 25, 28, 30, 31,32, 33, 34]  # Ajout de n=34
temps_chiclet = [0.00116, 0.00097, 0.00139, 0.00271, 0.07245, 3.16089, 29.96999, 122.73888, 247.33835,471.8901, 1831.49129, 4312.72]  # Ajustement pour n=34

# Données pour Ecotype (temps d'exécution parallèle)
n_ecotype = [11, 12, 13, 20, 25, 28, 30, 31, 32, 33, 34]
temps_ecotype = [0.000675, 0.001175, 0.001725, 0.05175, 2.53000, 24.85000, 102.10000, 206.35000, 418.75000, 1471.70172, 3872.88]

# Données pour Nova (temps d'exécution parallèle)
n_nova = [11, 12, 13, 20, 25, 28, 30, 31, 32, 33, 34]
temps_nova = [0.00100, 0.00159, 0.00303, 0.30676, 6.59223, 57.56923, 232.81625, 467.15962, 941.37095, 3157.91399, 5733.06168]

# Données pour Gros (temps d'exécution parallèle)
n_gros = [11, 12, 13, 20, 25, 28, 30, 31, 32, 33, 34]
temps_gros = [0.00053, 0.00134, 0.00210, 0.82623, 5.54115, 43.09201, 172.41347, 345.76354, 694.88420, 2825.84592, 5607.70724]
# (n=33, 64 processes, with -machinefile $OAR_NODEFILE)

# Création du graphique
plt.figure(figsize=(10, 6))

# Tracer la courbe pour Chiclet
plt.plot(n_chiclet, temps_chiclet, label='Chiclet', marker='o', color='blue')

# Tracer la courbe pour Ecotype
plt.plot(n_ecotype, temps_ecotype, label='Ecotype', marker='s', color='red')

# Tracer la courbe pour Nova
plt.plot(n_nova, temps_nova, label='Nova', marker='D', color='purple')

# Tracer la courbe pour Gros
plt.plot(n_gros, temps_gros, label='Gros', marker='h', color='green')

# Ajouter des labels et un titre
plt.xlabel('Valeur de n')
plt.ylabel('Temps d\'exécution (s)')
plt.title('Comparaison des temps d\'exécution (Chiclet, Ecotype, Nova, Gros)')

# Ajouter une légende
plt.legend()

# Afficher le graphique
plt.grid(True)
plt.show()

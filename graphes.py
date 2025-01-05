import matplotlib.pyplot as plt
import numpy as np
# Données pour Chiclet (temps d'exécution moyen)
n_chiclet = [10, 11, 12, 13, 20, 25, 28, 30, 31,32, 33, 34]  # Ajout de n=34
temps_chiclet = [0.00116, 0.00097, 0.00139, 0.00271, 0.07245, 3.16089, 29.96999, 122.73888, 247.33835,471.8901, 1831.49129, 4312.72]  # Ajustement pour n=34

# Données pour Ecotype (temps d'exécution parallèle)
n_ecotype = [11, 12, 13, 20, 25, 28, 30, 31, 32, 33, 34]
temps_ecotype = [0.000675, 0.001175, 0.001725, 0.05175, 2.53000, 24.85000, 102.10000, 206.35000, 418.75000, 1471.70172, 3872.88]

# Création du graphique
plt.figure(figsize=(10, 6))

# Tracer la courbe pour Chiclet
plt.plot(n_chiclet, temps_chiclet, label='Chiclet', marker='o', color='blue')

# Tracer la courbe pour Ecotype
plt.plot(n_ecotype, temps_ecotype, label='Ecotype', marker='s', color='red')

# Ajouter des labels et un titre
plt.xlabel('Valeur de n')
plt.ylabel('Temps d\'exécution (s)')
plt.title('Comparaison des temps d\'exécution (Chiclet vs Ecotype)')

# Ajouter une légende
plt.legend()

# Afficher le graphique
plt.grid(True)
plt.show()

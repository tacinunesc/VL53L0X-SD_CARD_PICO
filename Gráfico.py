# Código feito no google colab e designado para rodar no google colab
# Gráfico.py - Gráfico de Aceleração e Rotação
# Importação das bibliotecas necessárias
import pandas as pd
import matplotlib.pyplot as plt
from google.colab import files

# Upload do arquivo
print("Faça upload do arquivo CSV...")
uploaded = files.upload()
filename = list(uploaded.keys())[0]

# Ler e converter
df = pd.read_csv(filename)
df['t'] = df['numero_amostra'] / 75 # Valor experimental sujeito a erro
df[['ax','ay','az']] = df[['accel_x','accel_y','accel_z']] / 16384
df[['gx','gy','gz']] = df[['giro_x','giro_y','giro_z']] / 131

# Plotar
fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(10, 8))

ax1.plot(df['t'], df['ax'], 'r', label='X')
ax1.plot(df['t'], df['ay'], 'g', label='Y')
ax1.plot(df['t'], df['az'], 'b', label='Z')
ax1.set_ylabel('Aceleração (g)')
ax1.set_xlabel('Tempo (segundos)')
ax1.set_title('Acelerômetro')
ax1.legend()
ax1.grid(True)

ax2.plot(df['t'], df['gx'], 'r', label='X')
ax2.plot(df['t'], df['gy'], 'g', label='Y')
ax2.plot(df['t'], df['gz'], 'b', label='Z')
ax2.set_xlabel('Tempo (segundos)')
ax2.set_ylabel('Rotação (°/s)')
ax2.set_title('Giroscópio')
ax2.legend()
ax2.grid(True)

plt.tight_layout()
plt.show()

print(f"\nDuração: {df['t'].max():.1f}s | Amostras: {len(df)}")
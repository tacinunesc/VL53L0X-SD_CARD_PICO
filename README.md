# 📊 DataLogger com MPU6050 e Cartão SD na RP2040

<p align="center">Repositório dedicado ao sistema completo de coleta de dados inerciais utilizando a placa BitDogLab com RP2040, que monitora aceleração e velocidade angular através do sensor MPU6050, salvando os dados em cartão SD no formato CSV, com interface visual através de display OLED e feedback através de LED RGB e buzzer.</p>

## 📋 Apresentação da tarefa

Para este trabalho foi necessário implementar um sistema completo de coleta de dados inerciais que captura informações de aceleração (X, Y, Z) e velocidade angular (X, Y, Z) através do sensor MPU6050 via comunicação I2C. O sistema oferece interface de usuário através de display OLED SSD1306, controle por botões físicos, feedback visual através de LED RGB, feedback sonoro através de buzzer, e armazenamento seguro dos dados em cartão SD no formato CSV para posterior análise.

## 🎯 Objetivos

- Implementar coleta de dados inerciais completa com sensor MPU6050
- Criar sistema de armazenamento seguro em cartão SD formato CSV
- Implementar interface de usuário intuitiva através de display OLED
- Desenvolver sistema de controle através de botões físicos com debounce
- Implementar feedback visual através de LED RGB com códigos de cores
- Criar feedback sonoro através de buzzer para confirmações e alertas
- Estabelecer sistema de estados robusto para gerenciamento de operações
- Permitir montagem e desmontagem segura do sistema de arquivos
- Implementar numeração automática de arquivos para evitar sobrescrita

## 📚 Descrição do Projeto

Utilizou-se a placa BitDogLab com o microcontrolador RP2040 para criar um sistema profissional de coleta de dados inerciais. O sistema coleta dados de aceleração e velocidade angular através do sensor MPU6050 conectado via I2C0. As informações são exibidas em tempo real no display OLED SSD1306 conectado via I2C1.
O sistema implementa um robusto gerenciamento de cartão SD, incluindo detecção automática de inserção/remoção, montagem e desmontagem segura do sistema de arquivos FAT32. Os dados são salvos em formato CSV com cabeçalho descritivo, numeração automática de arquivos (data_0001.csv, data_0002.csv, etc.) e sincronização periódica para garantir integridade dos dados.
O controle do sistema é feito através de dois botões: Botão A para iniciar/parar gravação e Botão B para gerenciar montagem/desmontagem do cartão SD. O feedback é fornecido através de LED RGB com códigos de cores específicos para cada estado e buzzer com padrões sonoros distintos.
O display OLED apresenta informações contextuais baseadas no estado atual do sistema, incluindo status do cartão SD, nome do arquivo sendo gravado, contador de amostras coletadas e tempo decorrido de gravação.

## 🚶 Integrantes do Projeto
Matheus Pereira Alves

## 📑 Funcionamento do Projeto
O sistema opera através de uma máquina de estados bem definida organizada em um loop principal eficiente:

- Inicialização: Configuração de I2C dual (sensores e display), GPIOs, interrupções dos botões, LED RGB, buzzer e detecção inicial do cartão SD
- Leitura de Sensores: Coleta periódica (~75Hz) de dados de aceleração e velocidade angular do MPU6050 via registradores específicos
- Gerenciamento de SD: Funções automáticas de detecção, montagem, desmontagem e verificação de status do cartão SD
- Sistema de Estados: Máquina de estados robusta gerenciando INIT, READY, RECORDING, ERROR, SD_ACCESS, WAIT_SD_INSERT e WAIT_SD_REMOVE
- Armazenamento CSV: Gravação estruturada em formato CSV com cabeçalho, numeração automática e sincronização periódica para integridade
- Interface de Usuário: Display OLED contextual mostrando informações relevantes para cada estado do sistema
- Controle por Botões: Sistema de debounce via interrupções GPIO para controle preciso de gravação e gerenciamento de SD
- Feedback Multimodal: LED RGB com códigos de cores específicos e buzzer com padrões sonoros para diferentes eventos

## 👁️ Observações

- O sistema utiliza duas interfaces I2C separadas: I2C0 para sensor MPU6050 e I2C1 para display OLED;
- Implementa debounce robusto de 200ms nos botões através de sistema híbrido de interrupções e verificação no loop principal;
- O gerenciamento de cartão SD inclui reset completo do hardware antes de tentativas de montagem para evitar estados inconsistentes;
- Taxa de amostragem configurada para aproximadamente 75Hz para captura de movimentos;
- Sincronização forçada do arquivo a cada 50 amostras para garantir que dados sejam salvos mesmo em caso de falha;
- Sistema de numeração automática de arquivos com verificação de conflitos e limite de segurança de 9999 arquivos;
- Endereço do MPU6050 configurado para 0x68 (padrão);
- Código Grafico.py em python 3 executado no google colab para análise dos dados no domínio do tempo e com unidades convertidas

## :camera: GIF mostrando o funcionamento do programa na placa Raspberry Pi Pico
<p align="center">
  <img src="images/trabalhose12.gif" alt="GIF" width="526px" />
</p>
## ▶️ Vídeo no youtube mostrando o funcionamento do programa na placa Raspberry Pi Pico
<p align="center">
    <a href="https://youtu.be/S-TfhkOap-4">Clique aqui para acessar o vídeo</a>
</p>

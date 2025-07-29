
# DataLogger_AceleroGiro

Sistema embarcado baseado em Raspberry Pi Pico W para aquisição e registro de dados de aceleração e giroscópio via sensor MPU6050. Os dados são armazenados em cartão SD no formato CSV e posteriormente analisados com Python, utilizando Pandas e Matplotlib.

## Objetivo

Este projeto tem como finalidade o desenvolvimento de um **datalogger inercial** que captura informações do **sensor MPU6050**, armazenando os dados de aceleração e giroscópio em um **cartão microSD**, permitindo análises posteriores com ferramentas de visualização de dados.

## Hardware Utilizado

- Raspberry Pi Pico W (BitDogLab)
- IMU MPU6050 (via I2C)
- Cartão microSD de 16 GB (via SPI)
- Display OLED SSD1306
- LED RGB
- Buzzer (beep sonoro)
- 2 Push-buttons (interrupção via GPIO)

## Estrutura do Projeto

```
├── DataPlot/                 # Notebook Jupyter para análise dos dados CSV
├── lib/FatFs_SPI/           # Biblioteca FatFs para acesso ao cartão SD
├── lib_peripherals/         # Controle de display, buzzer, LED RGB, etc.
├── lib_sensors/             # Implementações para comunicação com o MPU6050
├── main.c                   # Código principal do firmware
├── hw_config.c              # Configuração de pinos e periféricos
├── mpu_axis.csv             # Exemplo de arquivo CSV gerado
├── CMakeLists.txt           # Configuração do build com CMake
├── pico_sdk_import.cmake    # Importação do SDK do Pico
```

## Funcionalidades

- Inicialização de periféricos ao ligar o sistema
- Comunicação com o IMU MPU6050 via I2C
- Montagem e desmontagem segura do cartão SD
- Registro de dados em tempo real (128 amostras, 100 ms de intervalo)
- Salvamento em CSV: tempo, amostra, aceleração (x,y,z), giroscópio (x,y,z)
- Feedback ao usuário por Display OLED, LED RGB e Buzzer
- Interação via terminal serial ou botões físicos

### Comandos via Terminal

| Comando | Função                              |
|---------|-------------------------------------|
| `a`     | Montar o cartão SD                  |
| `b`     | Desmontar o cartão SD               |
| `c`     | Listar arquivos no cartão           |
| `d`     | Mostrar o conteúdo de um arquivo    |
| `e`     | Exibir espaço livre no SD           |
| `f`     | Capturar dados do ADC e salvar      |
| `g`     | Iniciar captura do MPU6050 (CSV)    |
| `h`     | Exibir lista de comandos            |

### Funcionalidades dos Botões

- **Botão A:** Alterna entre montagem (`a`) e desmontagem (`b`) do cartão SD.
- **Botão B:** Inicia e interrompe a captura de dados do MPU6050 (`g`).

## Feedback Visual e Sonoro

- **Display OLED:** Exibe mensagens como “Cartão Montado!”, “Captura Finalizada!” etc.
- **LED RGB:**
  - Amarelo: Montagem/desmontagem
  - Verde: Sistema pronto
  - Azul piscando: Gravação em andamento
  - Vermelho: Captura ativa
  - Roxo piscando: Erro (ex: falha na montagem)
- **Buzzer:**
  - 1 beep curto: Início da captura
  - 2 beeps curtos: Fim da captura

## Análise dos Dados

O diretório `DataPlot/` contém um **notebook Jupyter** com o script para análise gráfica dos dados registrados. Os dados do arquivo `mpu_axis.csv` são lidos usando **Pandas** e visualizados com **Matplotlib**.

### Exemplo de gráficos gerados:

- Todos os eixos de aceleração e giroscópio em função do tempo
- Subplots por eixo
- Análise temporal de um único eixo

## Build e Execução

Este projeto utiliza **CMake** e o **Raspberry Pi Pico SDK**. Para compilar:

```bash
mkdir build
cd build
cmake ..
make
```

O firmware pode ser carregado na placa via arrasto de arquivo `.uf2` ou ferramentas como `picotool`.

## Vídeo de Demonstração:
https://www.youtube.com/playlist?list=PLaN_cHSVjBi9ACvQbGzQZg_UQJEiimDJV

## 📄 Licença

Este projeto está sob a licença MIT. Veja o arquivo `LICENSE` para mais detalhes.  

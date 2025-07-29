#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "hardware/adc.h"
#include "hardware/rtc.h"
#include "pico/stdlib.h"
#include "pico/time.h"

#include "ff.h"
#include "diskio.h"
#include "f_util.h"
#include "hw_config.h"
#include "my_debug.h"
#include "rtc.h"
#include "sd_card.h"

#include "lib_sensors/mpu6050.h"
#include "lib_peripherals/ssd1306.h"
#include "lib_peripherals/ledrgb.h"
#include "lib_peripherals/buzzer.h"

// =========== PERIFÉRICOS E DEFINIÇÕES ===========
#define ADC_PIN 26 // GPIO 26

// LED RGB
const uint led_rgb[] = {13, 11, 12}; 

// Buzzer
const uint BUZZER_PIN = 21;
const uint16_t PERIOD = 59609; // WRAP
const float DIVCLK = 16.0; // Divisor inteiro
static uint slice_21;
const uint16_t dc_values[] = {PERIOD * 0.3, 0}; // Duty Cycle de 30% e 0%

// I2C - Display SSD1306, MPU 6050
ssd1306_t ssd;
#define I2C_PORT i2c0
#define I2C_SDA 0
#define I2C_SCL 1
#define I2C_PORT_DISP i2c1
#define I2C_SDA_DISP 14
#define I2C_SCL_DISP 15
#define ENDERECO_DISP 0x3C
#define DISP_W 128
#define DISP_H 64

// Trecho para modo BOOTSEL com botão B
#include "pico/bootrom.h"
#define botaoB 6
#define botaoA 5
volatile uint32_t last_time = 0; // Para debounce

// Flags para ações dos botões
volatile bool flag_botao_a = false;
volatile bool flag_botao_b = false;
volatile bool sd_montado = false; // Indica se o SD será montado/ desmontado
volatile bool capturando = false; // Indica se a captura será iniciada/ encerrada


// =========== CONFIGURAÇÔES DO CARTÂO SD ===========
static bool logger_enabled;
static const uint32_t period = 1000;
static absolute_time_t next_log_time;

static char filename[20] = "mpu_axis.csv";

static sd_card_t *sd_get_by_name(const char *const name)
{
    for (size_t i = 0; i < sd_get_num(); ++i)
        if (0 == strcmp(sd_get_by_num(i)->pcName, name))
            return sd_get_by_num(i);
    DBG_PRINTF("%s: unknown name %s\n", __func__, name);
    return NULL;
}
static FATFS *sd_get_fs_by_name(const char *name)
{
    for (size_t i = 0; i < sd_get_num(); ++i)
        if (0 == strcmp(sd_get_by_num(i)->pcName, name))
            return &sd_get_by_num(i)->fatfs;
    DBG_PRINTF("%s: unknown name %s\n", __func__, name);
    return NULL;
}

static void run_setrtc()
{
    const char *dateStr = strtok(NULL, " ");
    if (!dateStr)
    {
        printf("Missing argument\n");
        return;
    }
    int date = atoi(dateStr);

    const char *monthStr = strtok(NULL, " ");
    if (!monthStr)
    {
        printf("Missing argument\n");
        return;
    }
    int month = atoi(monthStr);

    const char *yearStr = strtok(NULL, " ");
    if (!yearStr)
    {
        printf("Missing argument\n");
        return;
    }
    int year = atoi(yearStr) + 2000;

    const char *hourStr = strtok(NULL, " ");
    if (!hourStr)
    {
        printf("Missing argument\n");
        return;
    }
    int hour = atoi(hourStr);

    const char *minStr = strtok(NULL, " ");
    if (!minStr)
    {
        printf("Missing argument\n");
        return;
    }
    int min = atoi(minStr);

    const char *secStr = strtok(NULL, " ");
    if (!secStr)
    {
        printf("Missing argument\n");
        return;
    }
    int sec = atoi(secStr);

    datetime_t t = {
        .year = (int16_t)year,
        .month = (int8_t)month,
        .day = (int8_t)date,
        .dotw = 0, // 0 is Sunday
        .hour = (int8_t)hour,
        .min = (int8_t)min,
        .sec = (int8_t)sec};
    rtc_set_datetime(&t);
}

static void run_format()
{
    const char *arg1 = strtok(NULL, " ");
    if (!arg1)
        arg1 = sd_get_by_num(0)->pcName;
    FATFS *p_fs = sd_get_fs_by_name(arg1);
    if (!p_fs)
    {
        printf("Unknown logical drive number: \"%s\"\n", arg1);
        return;
    }
    /* Format the drive with default parameters */
    FRESULT fr = f_mkfs(arg1, 0, 0, FF_MAX_SS * 2);
    if (FR_OK != fr)
        printf("f_mkfs error: %s (%d)\n", FRESULT_str(fr), fr);
}
static void run_mount()
{
    const char *arg1 = strtok(NULL, " ");
    if (!arg1)
        arg1 = sd_get_by_num(0)->pcName;
    FATFS *p_fs = sd_get_fs_by_name(arg1);
    if (!p_fs)
    {
        printf("Unknown logical drive number: \"%s\"\n", arg1);
        return;
    }
    FRESULT fr = f_mount(p_fs, arg1, 1);
    if (FR_OK != fr)
    {
        printf("f_mount error: %s (%d)\n", FRESULT_str(fr), fr);

        // LED Roxo: pisca indicando que houve erro ao montar o cartão 
        for (int i = 0; i < 3; i++) {
            turn_on_purple((uint *)led_rgb);
            sleep_ms(250);
            turn_off_purple((uint *)led_rgb);
            sleep_ms(250);
        }
        return;
    }
    sd_card_t *pSD = sd_get_by_name(arg1);
    myASSERT(pSD);
    pSD->mounted = true;
    printf("Processo de montagem do SD ( %s ) concluído\n", pSD->pcName);
}
static void run_unmount()
{
    const char *arg1 = strtok(NULL, " ");
    if (!arg1)
        arg1 = sd_get_by_num(0)->pcName;
    FATFS *p_fs = sd_get_fs_by_name(arg1);
    if (!p_fs)
    {
        printf("Unknown logical drive number: \"%s\"\n", arg1);
        return;
    }
    FRESULT fr = f_unmount(arg1);
    if (FR_OK != fr)
    {
        printf("f_unmount error: %s (%d)\n", FRESULT_str(fr), fr);
        return;
    }
    sd_card_t *pSD = sd_get_by_name(arg1);
    myASSERT(pSD);
    pSD->mounted = false;
    pSD->m_Status |= STA_NOINIT; // in case medium is removed
    printf("SD ( %s ) desmontado\n", pSD->pcName);
}
static void run_getfree()
{
    const char *arg1 = strtok(NULL, " ");
    if (!arg1)
        arg1 = sd_get_by_num(0)->pcName;
    DWORD fre_clust, fre_sect, tot_sect;
    FATFS *p_fs = sd_get_fs_by_name(arg1);
    if (!p_fs)
    {
        printf("Unknown logical drive number: \"%s\"\n", arg1);
        return;
    }
    FRESULT fr = f_getfree(arg1, &fre_clust, &p_fs);
    if (FR_OK != fr)
    {
        printf("f_getfree error: %s (%d)\n", FRESULT_str(fr), fr);
        return;
    }
    tot_sect = (p_fs->n_fatent - 2) * p_fs->csize;
    fre_sect = fre_clust * p_fs->csize;
    printf("%10lu KiB total drive space.\n%10lu KiB available.\n", tot_sect / 2, fre_sect / 2);
}
static void run_ls()
{
    const char *arg1 = strtok(NULL, " ");
    if (!arg1)
        arg1 = "";
    char cwdbuf[FF_LFN_BUF] = {0};
    FRESULT fr;
    char const *p_dir;
    if (arg1[0])
    {
        p_dir = arg1;
    }
    else
    {
        fr = f_getcwd(cwdbuf, sizeof cwdbuf);
        if (FR_OK != fr)
        {
            printf("f_getcwd error: %s (%d)\n", FRESULT_str(fr), fr);
            return;
        }
        p_dir = cwdbuf;
    }
    printf("Directory Listing: %s\n", p_dir);
    DIR dj;
    FILINFO fno;
    memset(&dj, 0, sizeof dj);
    memset(&fno, 0, sizeof fno);
    fr = f_findfirst(&dj, &fno, p_dir, "*");
    if (FR_OK != fr)
    {
        printf("f_findfirst error: %s (%d)\n", FRESULT_str(fr), fr);
        return;
    }
    while (fr == FR_OK && fno.fname[0])
    {
        const char *pcWritableFile = "writable file",
                   *pcReadOnlyFile = "read only file",
                   *pcDirectory = "directory";
        const char *pcAttrib;
        if (fno.fattrib & AM_DIR)
            pcAttrib = pcDirectory;
        else if (fno.fattrib & AM_RDO)
            pcAttrib = pcReadOnlyFile;
        else
            pcAttrib = pcWritableFile;
        printf("%s [%s] [size=%llu]\n", fno.fname, pcAttrib, fno.fsize);

        fr = f_findnext(&dj, &fno);
    }
    f_closedir(&dj);
}
static void run_cat()
{
    char *arg1 = strtok(NULL, " ");
    if (!arg1)
    {
        printf("Missing argument\n");
        return;
    }
    FIL fil;
    FRESULT fr = f_open(&fil, arg1, FA_READ);
    if (FR_OK != fr)
    {
        printf("f_open error: %s (%d)\n", FRESULT_str(fr), fr);
        return;
    }
    char buf[256];
    while (f_gets(buf, sizeof buf, &fil))
    {
        printf("%s", buf);
    }
    fr = f_close(&fil);
    if (FR_OK != fr)
        printf("f_open error: %s (%d)\n", FRESULT_str(fr), fr);
}

#include "pico/time.h"  // Necessário para time_us_64()

// Função para capturar e salvar os dados de todos os eixos do MPU6050
void capture_mpu_data_and_save()
{
    // Buzzer: indica início da captura
    turn_on_buzzer(BUZZER_PIN, dc_values[0]);
    sleep_ms(1000);
    turn_off_buzzer(BUZZER_PIN);

    printf("\nCapturando dados do MPU6050. Aguarde finalização...\n");
    FIL file;
    FRESULT res = f_open(&file, filename, FA_WRITE | FA_CREATE_ALWAYS);
    if (res != FR_OK)
    {
        printf("\n[ERRO] Não foi possível abrir o arquivo para escrita. Monte o Cartao.\n");
        return;
    }

    // LED Verde: pronto para capturar
    turn_on_green((uint *) led_rgb);
    sleep_ms(1000);
    turn_off_green((uint *) led_rgb);

    // LED Vermelho: processo de leitura
    turn_on_red((uint *) led_rgb);

    // Cabeçalho CSV com número da amostra, o tempo da captura e os demais eixos
    const char *header = "Index,Time_ms,Accel_X,Accel_Y,Accel_Z,Gyro_X,Gyro_Y,Gyro_Z\n";
    UINT bw;
    res = f_write(&file, header, strlen(header), &bw);
    if (res != FR_OK)
    {
        printf("[ERRO] Não foi possível escrever cabeçalho no arquivo.\n");
        turn_on_purple((uint *)led_rgb);
        sleep_ms(500);
        turn_off_purple((uint *)led_rgb);
        f_close(&file);
        return;
    }

    // Marca o tempo inicial
    absolute_time_t start = get_absolute_time();

    for (int i = 0; i < 128; i++)
    {
        if (flag_botao_b && !capturando) { // Interrompe a captura de dados
            flag_botao_b = false;
            printf("[Botão B] Cancelando captura...\n");
            break;
        }
        int16_t accel[3], gyro[3], temp_unused;
        mpu6050_read_raw(I2C_PORT, MPU6050_DEFAULT_ADDR, accel, gyro, &temp_unused);  // Ignora temperatura

        // Tempo desde o início, em milissegundos
        int64_t elapsed_ms = absolute_time_diff_us(start, get_absolute_time()) / 1000;

        // Atualiza display com número da amostra
        char display_buffer[32];
        snprintf(display_buffer, sizeof(display_buffer), "Amostra: %d/128", i + 1);
        ssd1306_fill(&ssd, false);
        ssd1306_send_data(&ssd);
        ssd1306_draw_string(&ssd, display_buffer, 0, 0);
        ssd1306_send_data(&ssd);

        // Prepara linha CSV com dados
        char buffer[120];
        snprintf(buffer, sizeof(buffer), "%d,%lld,%d,%d,%d,%d,%d,%d\n",
                i + 1,
                elapsed_ms,
                accel[0], accel[1], accel[2],
                gyro[0], gyro[1], gyro[2]);


        res = f_write(&file, buffer, strlen(buffer), &bw);
        if (res != FR_OK)
        {
            printf("[ERRO] Não foi possível escrever no arquivo.\n");
            turn_on_purple((uint *)led_rgb);
            sleep_ms(500);
            turn_off_purple((uint *)led_rgb);
            f_close(&file);
            return;
        }

        sleep_ms(100);  // Ajuste para amostragem (ex: 100ms = 10 Hz)
    }

    f_close(&file);
    turn_off_red((uint *) led_rgb);

    // Buzzer: fim da captura
    for (int i = 0; i < 2; i++)
    {
        turn_on_buzzer(BUZZER_PIN, dc_values[0]);
        sleep_ms(500);
        turn_off_buzzer(BUZZER_PIN);
        sleep_ms(500);
    }

    printf("\nDados do MPU salvos no arquivo %s.\n\n", filename);
}


// Função para ler o conteúdo de um arquivo e exibir no terminal
void read_file(const char *filename)
{
    FIL file;
    FRESULT res = f_open(&file, filename, FA_READ);
    if (res != FR_OK)
    {
        // LED Roxo: indica erro
        turn_on_purple((uint *)led_rgb);
        sleep_ms(500);
        turn_off_purple((uint *)led_rgb);
        printf("[ERRO] Não foi possível abrir o arquivo para leitura. Verifique se o Cartão está montado ou se o arquivo existe.\n");
        return;
    }
    char buffer[128];
    UINT br;
    printf("Conteúdo do arquivo %s:\n", filename);
    while (f_read(&file, buffer, sizeof(buffer) - 1, &br) == FR_OK && br > 0)
    {
        buffer[br] = '\0';
        printf("%s", buffer);
    }
    f_close(&file);
    printf("\nLeitura do arquivo %s concluída.\n\n", filename);
}



// ============== INTERRUPÇÃO =============

void gpio_irq_handler(uint gpio, uint32_t events)
{
    uint32_t curr_time = to_ms_since_boot(get_absolute_time());

    if (curr_time - last_time > 200){
        last_time = curr_time;

        // Botão A: Alterna entre as ações: montar/ desmontar o cartão SD
        if (gpio == botaoA) {
            flag_botao_a = true;
            sd_montado = !sd_montado;
            return;
        }

        // Botão B: Alterna entre as ações: iniciar/ encerrar captura de dados
        else if (gpio == botaoB) {  
            flag_botao_b = true;
            capturando = !capturando;
            return;
        }
    }
}

static void run_help()
{
    printf("\nComandos disponíveis:\n\n");
    printf("Digite 'a' para montar o cartão SD\n");
    printf("Digite 'b' para desmontar o cartão SD\n");
    printf("Digite 'c' para listar arquivos\n");
    printf("Digite 'd' para mostrar conteúdo do arquivo\n");
    printf("Digite 'e' para obter espaço livre no cartão SD\n");
    printf("Digite 'f' para capturar dados do ADC e salvar no arquivo\n");
    printf("Digite 'g' para formatar o cartão SD\n");
    printf("Digite 'h' para exibir os comandos disponíveis\n");
    printf("\nEscolha o comando:  ");
}

typedef void (*p_fn_t)();
typedef struct
{
    char const *const command;
    p_fn_t const function;
    char const *const help;
} cmd_def_t;

static cmd_def_t cmds[] = {
    {"setrtc", run_setrtc, "setrtc <DD> <MM> <YY> <hh> <mm> <ss>: Set Real Time Clock"},
    {"format", run_format, "format [<drive#:>]: Formata o cartão SD"},
    {"mount", run_mount, "mount [<drive#:>]: Monta o cartão SD"},
    {"unmount", run_unmount, "unmount <drive#:>: Desmonta o cartão SD"},
    {"getfree", run_getfree, "getfree [<drive#:>]: Espaço livre"},
    {"ls", run_ls, "ls: Lista arquivos"},
    {"cat", run_cat, "cat <filename>: Mostra conteúdo do arquivo"},
    {"help", run_help, "help: Mostra comandos disponíveis"}};

static void process_stdio(int cRxedChar)
{
    static char cmd[256];
    static size_t ix;

    if (!isprint(cRxedChar) && !isspace(cRxedChar) && '\r' != cRxedChar &&
        '\b' != cRxedChar && cRxedChar != (char)127)
        return;
    printf("%c", cRxedChar); // echo
    stdio_flush();
    if (cRxedChar == '\r')
    {
        printf("%c", '\n');
        stdio_flush();

        if (!strnlen(cmd, sizeof cmd))
        {
            printf("> ");
            stdio_flush();
            return;
        }
        char *cmdn = strtok(cmd, " ");
        if (cmdn)
        {
            size_t i;
            for (i = 0; i < count_of(cmds); ++i)
            {
                if (0 == strcmp(cmds[i].command, cmdn))
                {
                    (*cmds[i].function)();
                    break;
                }
            }
            if (count_of(cmds) == i)
                printf("Command \"%s\" not found\n", cmdn);
        }
        ix = 0;
        memset(cmd, 0, sizeof cmd);
        printf("\n> ");
        stdio_flush();
    }
    else
    {
        if (cRxedChar == '\b' || cRxedChar == (char)127)
        {
            if (ix > 0)
            {
                ix--;
                cmd[ix] = '\0';
            }
        }
        else
        {
            if (ix < sizeof cmd - 1)
            {
                cmd[ix] = cRxedChar;
                ix++;
            }
        }
    }
}


// =========== PROGRAMA PRINCIPAL ===========
int main()
{
    // Para ser utilizado o modo BOOTSEL com botão A
    gpio_init(botaoA);
    gpio_set_dir(botaoA, GPIO_IN);
    gpio_pull_up(botaoA);
    gpio_set_irq_enabled_with_callback(botaoA, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);

    // Para ser utilizado o modo BOOTSEL com botão B
    gpio_init(botaoB);
    gpio_set_dir(botaoB, GPIO_IN);
    gpio_pull_up(botaoB);
    gpio_set_irq_enabled_with_callback(botaoB, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);

    init_ledrgb((uint *)led_rgb); // Inicialização do LED RGB

    init_buzzer(BUZZER_PIN, DIVCLK, PERIOD); // Inicialização do buzzer

    // Inicialização dos dispositivos com I2C
    i2c_init(I2C_PORT_DISP, 400 * 1000);
    gpio_set_function(I2C_SCL_DISP, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SDA_DISP, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_DISP);
    gpio_pull_up(I2C_SCL_DISP);
    ssd1306_init(&ssd, DISP_W, DISP_H, false, ENDERECO_DISP, I2C_PORT_DISP);
    ssd1306_config(&ssd);
    ssd1306_send_data(&ssd);

    ssd1306_fill(&ssd, false);
    ssd1306_send_data(&ssd);

    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    // Inicialização do MPU
    mpu6050_init(I2C_PORT, MPU6050_DEFAULT_ADDR);

    stdio_init_all();
    sleep_ms(5000);
    time_init();
    adc_init();

    // LED Amarelo: Indica que o sistema está inicializando
    turn_on_yellow((uint *)led_rgb);

    printf("FatFS SPI example\n");
    printf("\033[2J\033[H"); // Limpa tela
    printf("\n> ");
    stdio_flush();
    //    printf("A tela foi limpa...\n");
    //    printf("Depois do Flush\n");
    run_help();

    turn_off_yellow((uint *)led_rgb); // Desliga LED Amarelo
    turn_on_green((uint *)led_rgb);  // Sistema pronto

    while (true)
    {
        int cRxedChar = getchar_timeout_us(0);
        if (PICO_ERROR_TIMEOUT != cRxedChar)
            process_stdio(cRxedChar);

        if (cRxedChar == 'a' || (flag_botao_a && sd_montado)) // Monta o SD card se pressionar 'a', ou se pressionar o botão A
        {
            flag_botao_a = false;
            printf("\nMontando o SD...\n");
            ssd1306_fill(&ssd, false);
            ssd1306_send_data(&ssd);
            ssd1306_draw_string(&ssd, "Status: Montando SD", 0, 0);
            ssd1306_send_data(&ssd);

            turn_on_yellow((uint *)led_rgb);
            run_mount();
            turn_off_yellow((uint *)led_rgb);

            ssd1306_fill(&ssd, false);
            ssd1306_send_data(&ssd);
            ssd1306_draw_string(&ssd, "Status: SD Montado!", 0, 0);
            ssd1306_send_data(&ssd);
            
            sd_montado = true;
            printf("\nEscolha o comando (h = help):  ");
        }

        if (cRxedChar == 'b' || (flag_botao_a && !sd_montado)) // Desmonta o SD card se pressionar 'b', ou se pressionar o botão A
        {
            flag_botao_a = false;
            printf("\nDesmontando o SD. Aguarde...\n");
            ssd1306_fill(&ssd, false);
            ssd1306_send_data(&ssd);
            ssd1306_draw_string(&ssd, "Status: Desmontando...", 0, 0);
            ssd1306_send_data(&ssd);

            run_unmount();

            ssd1306_fill(&ssd, false);
            ssd1306_send_data(&ssd);
            ssd1306_draw_string(&ssd, "Status: SD Desmontado", 0, 0);
            ssd1306_send_data(&ssd);

            sd_montado = false;
            printf("\nEscolha o comando (h = help):  ");
        }

        if (cRxedChar == 'c') // Lista diretórios e arquivos
        {
            printf("\nListagem de arquivos no cartão SD.\n");
            ssd1306_fill(&ssd, false);
            ssd1306_send_data(&ssd);
            ssd1306_draw_string(&ssd, "Status: Listando SD...", 0, 0);
            ssd1306_send_data(&ssd);

            run_ls();

            ssd1306_fill(&ssd, false);
            ssd1306_send_data(&ssd);
            ssd1306_draw_string(&ssd, "Listagem concluída!", 0, 0);
            ssd1306_send_data(&ssd);

            printf("\nListagem concluída.\n");
            printf("\nEscolha o comando (h = help):  ");
        }

        if (cRxedChar == 'd') // Exibe o conteúdo do arquivo
        {
            ssd1306_fill(&ssd, false);
            ssd1306_send_data(&ssd);
            ssd1306_draw_string(&ssd, "Status: Lendo arquivo", 0, 0);
            ssd1306_send_data(&ssd);

            for (int i = 0; i < 3; i++) {
                turn_on_blue((uint *)led_rgb);
                sleep_ms(250);
                turn_off_blue((uint *)led_rgb);
                sleep_ms(250);
            }

            read_file(filename);

            ssd1306_fill(&ssd, false);
            ssd1306_send_data(&ssd);
            ssd1306_draw_string(&ssd, "Leitura concluída", 0, 0);
            ssd1306_send_data(&ssd);

            printf("Escolha o comando (h = help):  ");
        }

        if (cRxedChar == 'e') // Obtém o espaço livre no SD
        {
            ssd1306_fill(&ssd, false);
            ssd1306_send_data(&ssd);
            ssd1306_draw_string(&ssd, "Status: Verificando", 0, 0);
            ssd1306_draw_string(&ssd, "espaco no SD", 0, 10);
            ssd1306_send_data(&ssd);

            printf("\nObtendo espaço livre no SD.\n\n");
            run_getfree();

            ssd1306_fill(&ssd, false);
            ssd1306_send_data(&ssd);
            ssd1306_draw_string(&ssd, "Espaco verificado!", 0, 0);
            ssd1306_send_data(&ssd);

            printf("\nEspaço livre obtido.\n");
            printf("\nEscolha o comando (h = help):  ");
        }

        if (cRxedChar == 'f' || (flag_botao_b && capturando)) // Captura dados e salva no arquivo
        {
            flag_botao_b = false;
            capturando = true;
            ssd1306_fill(&ssd, false);
            ssd1306_draw_string(&ssd, "Status: Gravando...", 0, 0);
            ssd1306_draw_string(&ssd, "Coletando dados MPU", 0, 10);
            ssd1306_send_data(&ssd);

            for (int i = 0; i < 3; i++) {
                turn_on_blue((uint *)led_rgb);
                sleep_ms(250);
                turn_off_blue((uint *)led_rgb);
                sleep_ms(250);
            }

            capture_mpu_data_and_save();  // Mostra no display as amostras lá dentro

            ssd1306_fill(&ssd, false);
            ssd1306_draw_string(&ssd, "Dados salvos!", 0, 0);
            ssd1306_send_data(&ssd);

            capturando = false;
            printf("\nEscolha o comando (h = help):  ");
        }

        if (cRxedChar == 'g') // Formata o SD
        {
            printf("\nProcesso de formatação do SD iniciado. Aguarde...\n");
            ssd1306_fill(&ssd, false);
            ssd1306_draw_string(&ssd, "Status: Formatando...", 0, 0);
            ssd1306_send_data(&ssd);

            run_format();

            ssd1306_fill(&ssd, false);
            ssd1306_draw_string(&ssd, "SD formatado!", 0, 0);
            ssd1306_send_data(&ssd);

            printf("\nFormatação concluída.\n\n");
            printf("\nEscolha o comando (h = help):  ");
        }

        if (cRxedChar == 'h') // Exibe os comandos disponíveis
        {
            ssd1306_fill(&ssd, false);
            ssd1306_send_data(&ssd);
            ssd1306_draw_string(&ssd, "Comandos:", 0, 0);
            ssd1306_draw_string(&ssd, "a = Montar SD", 0, 10);
            ssd1306_draw_string(&ssd, "b = Desmontar SD", 0, 20);
            ssd1306_draw_string(&ssd, "f = Gravar MPU", 0, 30);
            ssd1306_send_data(&ssd);

            run_help();
        }

        sleep_ms(500);
    }
    return 0;

}
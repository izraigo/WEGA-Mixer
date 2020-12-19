# WEGA-Mixer

WEGA-Mixer это дозатор удобрений, который выполнен на базе ESP8266 и имеет веб интерфейс.

Целью данного дозатора, является автоматизированная дозировка необходимого количества(объема) удобрений из монорастворов.

*WEGA-Mixer* является отличным/важным дополнением к [HPG](https://github.com/siv237/HPG) калькулятору из которого можно дозировать(печатать) раствор на прямую.

Так же *WEGA-Mixer* является важным звеном в [WEGA ecosystem](images/wega-ecosystem.jpg)

Содержание
=================
<!--ts-->
* [Install software](#install)
    * [Arduino IDE](#arduino)
    * [Visual Studio Code configuration](#vscode)
    * [Проверка правильности выбранных настроек](#config-check)
    * [Проверка ESP и простой веб сервера](#esp-check)
* [Подключение по схеме компонентов для миксера](#connections)
* [Mixer(Dosator)](#mixer)
* [Калибровка весов/стола](#scale-calibration)
* [Важно](#important)

<!--te-->

<a name="install"></a>
### Install software

<a name="arduino"></a>
#### Arduino IDE
[Install Arduino IDE](https://www.arduino.cc/en/software)

Запускаем Arduino IDE и устанавливаем все необходимые библиотеки

Tools -> Manage libraries -> search libraries

- LiquidCrystal_I2C.h
- Adafruit_MCP23017.h
- HX711.h

Вы можете пропустить этот шаг если вы собираетесь использовать vscode IDE как основной

Выбираем правильную плату ESP

Tools -> boards manager -> ESP8266 -> NodeMCU 1.0(ESP-12E)

<a name="vscode"></a>
#### Visual Studio Code configuration
<details>
<p>
Если вы планируете использовать vscode как основную IDE для работы с файлами ардуины вам потребуется установить [arduino plugin](https://marketplace.visualstudio.com/items?itemName=vsciot-vscode.vscode-arduino) и сконфигурировать его для работы с вашей платой.

[Здесь видео пример как это сделать](https://www.youtube.com/watch?v=FnEvJXpxxNM)
</p>
</details>

<a name="config-check"></a>
#### Проверка правильности выбранных настроек
Будем считать, что вы уже сконфигурировали вашу IDE для работы с платой. Теперь попробуем загрузить простой пример, маргание светодиодом.

File -> examples -> basics -> blink
Нажать upload/загрузить

Как только процесс загрузки закончится вы должны увидеть как светодиод маргает на вашей ESP

<a name="esp-check"></a>
#### Проверка ESP и простой веб сервера
- Открыть файл `web-ota-example` 
- Изменить значения в файле на ваши
```bash
const char* ssid = "YOUR_WIFI_NETWORK_NAME";
const char* password = "YOUR_WIFI_PASSWORD";
```

- Теперь можно заливать код на вашу плату
- Нажать upload/загрузить

Как только процесс загрузки закончится веб сервер должен быть запущен, теперь необходимо найти IP, который получила плата.
Это можно сделать на странице конфигурации вашего роутера.
Рекомендуется прописать резервацию для этого IP, в этом случае каждый раз ваша плата будет получать один и тот же адрес.

<a name="connections"></a>
### Подключение по схеме компонентов для миксера
Здесь можно видеть текущую схему подключения

<a href="images/connection_diagramm.png"><img src="images/connection_diagramm.png" width="250"></a>

Здесь находится [диаграмма](scheme)

<a name="mixer"></a>
### Mixer(Dosator)
Последняя версия [миксера](mixer)

- Открываем файл прошивки из папки `mixer/mixer.ino`
- Обновляем `ssid` и `password` для вашего WiFi.

```bash
const char* ssid = "YOUR_WIFI_NETWORK_NAME";
const char* password = "YOUR_WIFI_PASSWORD";
```

- Обновляем имена насосов, если это требуется. Если вы собирали точто по схеме, то скорее всего ничего обновлять не нужно.
- Пример имен насосов в коде:

```bash
a0=0 .. a7=7
b0=8 .. b7=15
```

Это означает, если вы подключаете `B0/A0`, то имена будут `8/0`, еще один пример `B3/A3 == 11/3`.
Почему так? На плате написаны номера портов `A0`, `A1...B7`, но библиотеке безразличны эти имена и она назначает портам очередность с нуля до 15.

Так как этих плат может быть несколько 2, 3 и до 32 и все порты имеют сквозную нумерацию.

```bash
// Pump #1
int a0 = 0;
int b0 = 8;
...
etc.
```

- Сохраняем код
- Нажать upload/загрузить

Как только код будет загружен на плату и произойдет перезагрузка, на экране должна появится надпись `Ready`

<a href="images/screen_ready.png"><img src="images/screen_ready.png" width="250"></a>

Ну все, код залит, экран работает.

<a name="scale-calibration"></a>
### Калибровка весов/стола

1. Нужно взять точно известный вес в районе 500 грамм
2. Отметить две точки на которых будут стоять бутылки А и B
3. Перезагрузить систему с пустым столом.
4. Поставить на точку А известный вес и подбирать значение `scale_calibration`

Если вес ниже эталлонного то значение `scale_calibration` уменьшаем

Например:

- scale_calibration = 2045
- эталлоный вес 47.90
- ставим его на весы(стол) и если значение ниже эталонного
- вес на столе 47.50
- scale_calibration уменьшаем
- scale_calibration = 2040
- прошиваем
- повторяем процедуру
- ставим эталлоный вес на весы(стол) и если значение ниже эталонного
- вес на столе 47.64
- scale_calibration уменьшаем
- scale_calibration = 2040


<a name="important"></a>
### Важно

Есть несколько ньюансов, которые необходио учитывать.

1. Если вы планируете часто делать(печатать) растворы на объем до 10 литров, то стоит изменить концентрацию ваших монорастворов таким образом, что бы значения в поле гр.жидкости были больше 1.5 грамм.
Это позволит делать растворы гораздо быстрее.
Почему так? потому что в логике миксера заложенно по капельное дозирование, если вес желаемой дозы удобрений меньше 1.5 грамм.

<a href="images/small_portions.png"><img src="images/small_portions.png" width="250"></a>

2. В миксере заложенно делать прилоад удобрений перед основной подачей. Что это значит. Это значит, что миксер набирает некоторое количество удобрений в трубки, перед основной подачей. В данный момент все значения разные, так как они будут для каждого случая свои. Это зависит от длины ваших шлангов. В некоторых случая, шланги короткие, в некоторых длинные 30см и более. Для разных шлангов разное время подготовки(прилоад) удобрений.
Если вы видите, что прилоад удобрений избыточен или не достаточен, вам необходимо изменить [вот эти значения](https://github.com/siv237/WEGA-Mixer/blob/readme_update/mixer/mixer.ino#L232-L244) для тех насосов, которые необходимо скорректировать. 
Ниже приведен пример, где искать эти значения.

```bash
scale.set_scale(scale_calibration); //A side
  p1=pumping(v1, pump1,pump1r, "1:Ca", 5000);
  p2=pumping(v2, pump2,pump2r, "2:KN", 5000);
  p3=pumping(v3, pump3,pump3r, "3:NH", 7000);

  ...
```


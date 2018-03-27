# Драйвер NMEA датчиков

Драйвер предназначен для приёма данных от NMEA совместимых датчиков
по последовательной линии (UART) и по протоколу UDP/IP.

# Сборка

Драйвер поддерживает сборку для следующих платформ:

- Linux/GCC Intel 32/64 бита;
- Linux/Clang Intel 32/64 бита;
- Linux/GCC ARM 32/64 бита;
- Linux/Clang ARM 32/64 бита;
- MSYS2/GCC 32/64 бита;
- MSYS2/Clang 32/64 бита;
- Visual Studio 2013 32/64 бита;
- Visual Studio 2015 32/64 бита.

Для сборки необходимы следующие программы и библиотеки:

- make (только для Linux и MSYS2);
- CMake версии 2.8 и выше;
- GLib версии 2.40 и выше;
- Libxml версии 2.X.

## Сборка с использованием компиляторов GCC и CLang

Сборка осуществляется командой make в корневом каталоге библиотеки. При
этом будет создан каталог build, в котором будут размещаться все временные
файлы. Собранная библиотека и тесты будут размещены в каталоге bin. По
умолчанию осуществляется сборка рабочей версии. Для сборки отладочной
версии необходимо указать параметр debug.

```bash
 $ make
 $ make debug
```

При сборке драйвера с использованием 32-х битного компилятора GCC для
процессоров Intel, необходимо указать использовать инструкции SSE для
математических операций. Для этого следует определить переменную окружения
CFLAGS="-msse -mfpmath=sse" перед сборкой.

```bash
 $ CFLAGS="-msse -mfpmath=sse" make
```

При сборке драйвера для процессоров ARM, необходимо указать использовать
инструкции NEON для математических операций. Для этого следует определить
переменную окружения CFLAGS="-mfpu=neon" перед сборкой.

```bash
 $ CFLAGS="-mfpu=neon" make
```

Для установки драйвера необходимо использовать параметр install. Переменная
окружения DESTDIR задаёт местоположение для устанавливаемых компонентов.
По умолчанию используются значения специфичные для целевой ОС.

## Сборка с использованием Visual Studio

Перед сборкой необходимо создать каталог build, в котором будут размещаться
временные файлы. В этом каталоге необходимо запустить программу cmake-gui.

```
 c:\src\hyscannmeadrv\build> cmake-gui ..
```

Затем необходимо выбрать компилятор и целевую платформу и сгенерировать
проект (solution) для Visual Studio. 

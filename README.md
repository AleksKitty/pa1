# pa2

Мы такие:
1) бодро стартуем программку 
2) проверяем исходные данные 
3) создаем все pipe (каждый с каждым) и все остальные процессики (теперь в структуре еще поле денег появилось)
4) Все дочерние процессики отправляют всем сообщение STARTED
5) И получают их, соотвественно.
Эта часть вроде работает.

Далее: 
4) Когда родительский процесс получает все сообщения STARTED, то он вызывает функцию bank_robbery(Родительский процесс, число детей). 
Эта функция (она дана) (см. bank_robbery.c) вызывает ряд функций transfer (см. pa23.c). Функция transfer должна отпралять деньги от одного процесса к другому. 

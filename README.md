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

Из задания "При выполнении перевода родительский процесс отправляет сообщение TRANSFER процессу «Csrc», после чего переходит в
режим ожидания подтверждения (пустое сообщение типа ACK) процессом «Сdst» получения перевода. Переводы могут быть инициированы только родительсим процессом".

Функцию transfer как-то написала. На данный момент в transfer родитель отправляет сообщение Csrc. А потом должен ждать сообщение от Сdst.

Сами деньги пока нигде не изменятся.

5) Теперь в месте, где живут процессы (ВОПРОС: там же под fork? или как-то выносить?), необходимо проверять на тип полученного сообщения (TRANSFER) и изменять баланс.


Это не конец, если что. В следущих сериях не пропустите - сообшения DONE и STOP, добавление BalanceHistory, а также появление всеми любимых моментов времени. 

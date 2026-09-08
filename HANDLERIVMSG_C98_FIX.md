# Naprawa handlePrivmsg() - C++98 Compatible

## ❌ PROBLEMY W AKTUALNYM KODZIE

```cpp
void Server::handlePrivmsg(Parser& parser, int clientFd){
    // ...
    
    // for (std::vector<std::string>::iterator it = params.begin(); it != params.end(); it++){
    // }  ← PUSTA PĘTLA!
    
    while(size_t spacePos = (findTokenEnd(recipient, ":"))){  // ❌ recipient nie istnieje!
        // ❌ Logika copy-paste z gdzieś indziej
    }
}
```

---

## ✅ PRAWIDŁOWA IMPLEMENTACJA - C++98

```cpp
void Server::handlePrivmsg(Parser& parser, int clientFd){

    const std::vector<std::string> &params = parser.getParams();
    Client &client = getClient(clientFd);
    const std::string &msg = parser.getTrailing();
    
    // Walidacja
    if (params.empty() || msg.empty())
    {
        client.sendMsg(":server 461 " + client.getNickname() + " PRIVMSG :Not enough parameters\r\n");
        return;
    }

    // Przygotuj wiadomość do wysłania
    std::string formattedMsg = ":" + client.getNickname() + "!" + client.getUsername()
        + "@" + client.getHostName() + " PRIVMSG ";
    
    // Oddziel kanały od użytkowników
    std::vector<std::string> channels;      // Kanały do wysłania (sprawdzić czy klient jest członkiem)
    std::vector<std::string> users;         // Użytkownicy do wysłania
    
    // ✅ ITERUJ PO VECTOR PARAMS (już sparsowany przez Parser!)
    for (std::vector<std::string>::const_iterator it = params.begin(); it != params.end(); ++it)
    {
        const std::string &recipient = *it;  // Każdy recipient z params
        
        // Rozpoznaj czy to kanał czy użytkownik
        char firstChar = recipient[0];
        
        if (firstChar == '#' || firstChar == '&' || firstChar == '+' || firstChar == '!')
        {
            // To jest KANAŁ
            channels.push_back(recipient);
        }
        else
        {
            // To jest UŻYTKOWNIK
            users.push_back(recipient);
        }
    }
    
    // === WYSYŁANIE DO KANAŁÓW ===
    const std::set<std::string> &userChannels = client.getChannels();
    
    for (std::vector<std::string>::const_iterator it = channels.begin(); it != channels.end(); ++it)
    {
        const std::string &channelName = *it;
        
        // Sprawdź czy klient jest członkiem kanału
        if (userChannels.find(channelName) == userChannels.end())
        {
            // Nie jest członkiem
            client.sendMsg(":server 404 " + client.getNickname() + " " + channelName + " :Cannot send to channel\r\n");
            continue;
        }
        
        // Wyślij do wszystkich członków kanału
        try
        {
            Channel &channel = getChannel(channelName);
            std::string channelMsg = formattedMsg + channelName + " :" + msg + "\r\n";
            sendMsgToChannel(&channel, channelMsg, clientFd);
        }
        catch (const std::exception &e)
        {
            client.sendMsg(":server 403 " + client.getNickname() + " " + channelName + " :No such channel\r\n");
        }
    }
    
    // === WYSYŁANIE DO UŻYTKOWNIKÓW ===
    for (std::vector<std::string>::const_iterator it = users.begin(); it != users.end(); ++it)
    {
        const std::string &targetNick = *it;
        
        // Znajdź użytkownika po nicku
        Client *targetClient = findClientByNickname(targetNick);
        
        if (targetClient == NULL)
        {
            // Użytkownik nie istnieje
            client.sendMsg(":server 401 " + client.getNickname() + " " + targetNick + " :No such nick/channel\r\n");
            continue;
        }
        
        // Wyślij wiadomość do użytkownika
        std::string userMsg = formattedMsg + targetNick + " :" + msg + "\r\n";
        targetClient->sendMsg(userMsg);
    }
}
```

---

## 📝 KLUCZOWE PUNKTY C++98

### ✅ Iterator zamiast `auto`:
```cpp
// ❌ C++11+ (nie masz tego)
for (const auto &recipient : params) { }

// ✅ C++98 (prawidłowo)
for (std::vector<std::string>::const_iterator it = params.begin(); it != params.end(); ++it)
{
    const std::string &recipient = *it;
    // Teraz możesz używać recipient
}
```

### ✅ Jawne typy:
```cpp
// ❌ Brak jawnego typu
const auto& firstChar = recipient[0];

// ✅ Jawny typ
char firstChar = recipient[0];
```

### ✅ Warunki zamiast ternary:
```cpp
// ❌ Zbyt zwinny styl
bool isChannel = (firstChar == '#' || firstChar == '&' || firstChar == '+' || firstChar == '!');

// ✅ Jawny, czytelny warunek
if (firstChar == '#' || firstChar == '&' || firstChar == '+' || firstChar == '!')
{
    channels.push_back(recipient);
}
```

### ✅ Try-catch dla wyjątków:
```cpp
// Funkcja getChannel() może wyrzucić exception
try
{
    Channel &channel = getChannel(channelName);
    sendMsgToChannel(&channel, channelMsg, clientFd);
}
catch (const std::exception &e)
{
    // Obsłuż błąd
    client.sendMsg(":server 403 ...");
}
```

### ✅ Porównanie pointera z NULL:
```cpp
// ❌ C++11+
if (targetClient == nullptr) { }

// ✅ C++98
if (targetClient == NULL) { }
```

---

## 🔄 PORÓWNANIE STAREGO vs NOWEGO

| Funkcjonalność | Stary kod | Nowy kod |
|---|---|---|
| **Parsowanie recipientów** | Pusta pętla + `recipient` undefined | Pętla po `params` vector |
| **Rozpoznanie kanału/user** | Nie było | `if (firstChar == '#')` |
| **Wysłanie do kanału** | Logika copy-paste | Funkcja `sendMsgToChannel()` |
| **Wysłanie do użytkownika** | Nie było | `findClientByNickname()` |
| **Błędy** | Milczone | Jawne odpowiedzi IRC (401, 403, 404) |
| **C++98 kompatybilność** | ❌ | ✅ |

---

## 🎯 DODATKOWO: USTAW handlePrivmsg() w executeCommand()

Pamiętaj aby dodać nową komendę do `executeCommand()`:

```cpp
void Server::executeCommand(Parser& parser, int clientFd)
{
    const std::string &command = parser.getCommand();

    if (command == "PASS")
        handlePass(parser, clientFd);
    else if (command == "NICK")
        handleNick(parser, clientFd);
    else if (command == "USER")
        handleUser(parser, clientFd);
    else if (command == "PRIVMSG")    // ← DODAJ TO!
        handlePrivmsg(parser, clientFd);
    // TODO: Pozostałe komendy...
}
```

---

## 📋 CZYSTA LISTA ZMIAN

1. **Usuń** niezdefiniowaną logikę `while(size_t spacePos = ...)`
2. **Zaktualizuj** pętlę for aby rzeczywiście iterowała po `params`
3. **Dodaj** separację kanałów od użytkowników
4. **Dodaj** wysyłanie do kanałów (z walidacją członkostwa)
5. **Dodaj** wysyłanie do użytkowników (z walidacją istnienia)
6. **Zachowaj** C++98 kompatybilność (iteratory, jawne typy, NULL)

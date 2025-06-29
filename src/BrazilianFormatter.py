#!/usr/bin/env python3
"""
File: BrazilianFormatter.py
Author: Omar Achraf / omarachraf@gmail.com
Date: 2025-06-28
Description: Formatador de números no padrão brasileiro para interface GUI.
             Responsible for converting numeric values to Brazilian locale format
             with proper thousands separator (.) and decimal separator (,).
"""

import locale
import re


class BrazilianFormatter:
    """
    Class responsible for formatting numbers according to Brazilian standards.
    Provides methods for integer and decimal formatting with proper separators.
    Implements fallback mechanisms for different operating systems.
    """
    
    def __init__(self):
        header = "BrazilianFormatter.__init__()"
        self.locale_set = False
        self.setup_locale()
    
    
    """
    Setup Brazilian locale with fallback options.
    """
    def setup_locale(self):
        header = "BrazilianFormatter.setup_locale()"
        
        # Lista de locales brasileiros para tentar
        br_locales = [
            'pt_BR.UTF-8',          # Linux/Mac padrão
            'pt_BR',                # Linux/Mac alternativo
            'Portuguese_Brazil.1252', # Windows
            'Portuguese_Brazil',     # Windows alternativo
            'pt-BR',                # Algumas distribuições
            'C'                     # Fallback universal
        ]
        
        for loc in br_locales:
            try:
                locale.setlocale(locale.LC_ALL, loc)
                self.locale_set = True
                print(f"{header}: Brazilian locale set to '{loc}'")
                break
            except locale.Error:
                continue
        
        if not self.locale_set:
            print(f"{header}: Could not set Brazilian locale, using manual formatting")
    
    
    """
    Format integer number with Brazilian thousands separator.
    Args:
        number - Integer number to format
    Returns:
        str - Formatted number string (ex: 1.234.567)
    """
    def format_integer(self, number):
        try:
            if self.locale_set:
                # Usar locale se disponível
                return locale.format_string("%.0f", number, grouping=True)
            else:
                # Formatação manual
                return self.manual_format_integer(number)
        except Exception:
            return self.manual_format_integer(number)
    
    
    """
    Format decimal number with Brazilian separators.
    Args:
        number - Decimal number to format
        decimals - Number of decimal places (default: 2)
    Returns:
        str - Formatted number string (ex: 1.234.567,89)
    """
    def format_decimal(self, number, decimals=2):
        try:
            if self.locale_set:
                # Usar locale se disponível
                format_str = f"%.{decimals}f"
                return locale.format_string(format_str, number, grouping=True)
            else:
                # Formatação manual
                return self.manual_format_decimal(number, decimals)
        except Exception:
            return self.manual_format_decimal(number, decimals)
    
    
    """
    Manual integer formatting for systems without Brazilian locale.
    Args:
        number - Integer number to format
    Returns:
        str - Manually formatted number string
    """
    def manual_format_integer(self, number):
        try:
            # Converter para string e inverter
            num_str = str(int(abs(number)))
            reversed_str = num_str[::-1]
            
            # Adicionar pontos a cada 3 dígitos
            groups = []
            for i in range(0, len(reversed_str), 3):
                groups.append(reversed_str[i:i+3])
            
            # Juntar grupos com ponto e inverter novamente
            formatted = '.'.join(groups)[::-1]
            
            # Adicionar sinal negativo se necessário
            if number < 0:
                formatted = '-' + formatted
                
            return formatted
            
        except Exception as e:
            print(f"Error in manual_format_integer: {e}")
            return str(number)
    
    
    """
    Manual decimal formatting for systems without Brazilian locale.
    Args:
        number - Decimal number to format
        decimals - Number of decimal places
    Returns:
        str - Manually formatted number string
    """
    def manual_format_decimal(self, number, decimals=2):
        try:
            # Separar parte inteira e decimal
            integer_part = int(abs(number))
            decimal_part = abs(number) - integer_part
            
            # Formatar parte inteira
            formatted_integer = self.manual_format_integer(integer_part)
            
            # Formatar parte decimal
            decimal_str = f"{decimal_part:.{decimals}f}"[2:]  # Remove '0.'
            
            # Juntar com vírgula
            result = f"{formatted_integer},{decimal_str}"
            
            # Adicionar sinal negativo se necessário
            if number < 0:
                result = '-' + result.lstrip('-')
                
            return result
            
        except Exception as e:
            print(f"Error in manual_format_decimal: {e}")
            return f"{number:.{decimals}f}"
    
    
    """
    Format RPM value with appropriate precision.
    Args:
        rpm - RPM value to format
    Returns:
        str - Formatted RPM string
    """
    def format_rpm(self, rpm):
        if rpm == 0:
            return "0 rpm"
        elif rpm < 1000:
            return f"{str(int(rpm))} rpm"
        else:
            return f"{self.format_integer(rpm)} rpm"
    
    
    """
    Format frequency value with appropriate precision.
    Args:
        frequency - Frequency value in Hz
    Returns:
        str - Formatted frequency string
    """
    def format_frequency(self, frequency):
        if frequency == 0:
            return "0 Hz"
        elif frequency < 1000:
            return f"{frequency:.1f} Hz".replace('.', ',')
        else:
            return f"{self.format_integer(frequency)} Hz"
    
    
    """
    Format time interval in microseconds.
    Args:
        microseconds - Time value in microseconds
    Returns:
        str - Formatted time string with unit
    """
    def format_time_microseconds(self, microseconds):
        if microseconds == 0:
            return "0 µs"
        elif microseconds < 1000:
            return f"{int(microseconds)} µs"
        elif microseconds < 1000000:
            # Converter para milissegundos
            ms = microseconds / 1000
            return f"{ms:.1f} ms".replace('.', ',')
        else:
            # Converter para segundos
            s = microseconds / 1000000
            return f"{s:.2f} s".replace('.', ',')
    
    
    """
    Format revolution counter with thousands separators.
    Args:
        revolutions - Total revolution count
    Returns:
        str - Formatted revolution string
    """
    def format_revolutions(self, revolutions):
        if revolutions == 0:
            return "0 rev"
        else:
            return f"{self.format_integer(revolutions)} rev"


# Instância global para uso fácil
formatter = BrazilianFormatter()


"""
Convenience functions for direct use in GUI.
"""

"""Format RPM in Brazilian style."""
def format_rpm_br(rpm):
    return formatter.format_rpm(rpm)

"""Format frequency in Brazilian style.""" 
def format_frequency_br(frequency):
    return formatter.format_frequency(frequency)

"""Format time in Brazilian style."""
def format_time_br(microseconds):
    return formatter.format_time_microseconds(microseconds)

"""Format revolutions in Brazilian style."""
def format_revolutions_br(revolutions):
    return formatter.format_revolutions(revolutions)

"""Format integer in Brazilian style."""
def format_integer_br(number):
    return formatter.format_integer(number)

"""Format decimal in Brazilian style."""
def format_decimal_br(number, decimals=2):
    return formatter.format_decimal(number, decimals)